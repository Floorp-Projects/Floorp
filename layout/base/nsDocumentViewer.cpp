/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/* container for a document and its presentation */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentViewerPrint.h"

#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsICSSStyleSheet.h"
#include "nsIFrame.h"

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
#include "nsContentUtils.h"
#include "nsLayoutStylesheetCache.h"

#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsView.h" // For nsView::GetViewFor

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
#include "nsIParser.h"
#include "nsGUIEvent.h"
#include "nsHTMLReflowState.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIImageLoadingContent.h"
#include "nsCopySupport.h"
#include "nsIDOMHTMLFrameSetElement.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif
#include "nsPrintfCString.h"

#include "nsIClipboardHelper.h"

#include "nsPIDOMWindow.h"
#include "nsJSEnvironment.h"
#include "nsIFocusController.h"
#include "nsIMenuParent.h"

#include "nsIScrollableView.h"
#include "nsIHTMLDocument.h"
#include "nsITimelineService.h"
#include "nsGfxCIID.h"
#include "nsStyleSheetService.h"

#include "nsIPrompt.h"
#include "imgIContainer.h" // image animation mode constants

//--------------------------
// Printing Include
//---------------------------
#ifdef NS_PRINTING

#include "nsIWebBrowserPrint.h"

#include "nsPrintEngine.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIPrintOptions.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

// PrintOptions is now implemented by PrintSettingsService
static const char sPrintOptionsContractID[]         = "@mozilla.org/gfx/printsettings-service;1";

// Printing Events
#include "nsPrintPreviewListener.h"

#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIPluginDocument.h"

// Print Progress
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"

// Print error dialog
#include "nsIWindowWatcher.h"

// Printing 
#include "nsPrintEngine.h"
#include "nsPagePrintTimer.h"

#endif // NS_PRINTING

// FrameSet
#include "nsIDocument.h"

//focus
#include "nsIDOMEventReceiver.h"
#include "nsIDOMFocusListener.h"
#include "nsISelectionController.h"

#include "nsBidiUtils.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIWebNavigation.h"
#include "nsWeakPtr.h"
#include "nsEventDispatcher.h"

//paint forcing
#include "prenv.h"
#include <stdio.h>

//switch to page layout
#include "nsGfxCIID.h"

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
                           public nsIDocumentViewerPrint

#ifdef NS_PRINTING
                           , public nsIWebBrowserPrint
#endif

{
  friend class nsDocViewerSelectionListener;
  friend class nsPagePrintTimer;
  friend class nsPrintEngine;

public:
  DocumentViewerImpl(nsPresContext* aPresContext);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIDocumentViewer interface...
  NS_IMETHOD SetUAStyleSheet(nsIStyleSheet* aUAStyleSheet);
  NS_IMETHOD GetDocument(nsIDocument** aResult);
  NS_IMETHOD GetPresShell(nsIPresShell** aResult);
  NS_IMETHOD GetPresContext(nsPresContext** aResult);
  NS_IMETHOD CreateDocumentViewerUsing(nsPresContext* aPresContext,
                                       nsIDocumentViewer** aResult);

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

  // nsIMarkupDocumentViewer
  NS_DECL_NSIMARKUPDOCUMENTVIEWER

#ifdef NS_PRINTING
  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT
#endif

  typedef void (*CallChildFunc)(nsIMarkupDocumentViewer* aViewer,
                                void* aClosure);
  void CallChildren(CallChildFunc aFunc, void* aClosure);

  // nsIDocumentViewerPrint Printing Methods
  NS_DECL_NSIDOCUMENTVIEWERPRINT

protected:
  virtual ~DocumentViewerImpl();

private:
  nsresult MakeWindow(nsIWidget* aParentWidget,
                      const nsRect& aBounds);
  nsresult InitInternal(nsIWidget* aParentWidget,
                        nsISupports *aState,
                        nsIDeviceContext* aDeviceContext,
                        const nsRect& aBounds,
                        PRBool aDoCreation,
                        PRBool aInPrintPreview,
                        PRBool aNeedMakeCX = PR_TRUE);
  nsresult InitPresentationStuff(PRBool aDoInitialReflow);

  nsresult GetPopupNode(nsIDOMNode** aNode);
  nsresult GetPopupLinkNode(nsIDOMNode** aNode);
  nsresult GetPopupImageNode(nsIImageLoadingContent** aNode);

  void HideViewIfPopup(nsIView* aView);

  void DumpContentToPPM(const char* aFileName);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

  nsresult GetDocumentSelection(nsISelection **aSelection);

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

  nsWeakPtr mContainer; // it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;   // ??? can't hurt, but...

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // ??? should we really own it?
  nsCOMPtr<nsIViewManager> mViewManager;
  nsCOMPtr<nsPresContext> mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsIStyleSheet>  mUAStyleSheet;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  nsCOMPtr<nsIDOMFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;
  nsCOMPtr<nsISHEntry> mSHEntry;

  nsIWidget* mParentWidget;          // purposely won't be ref counted

  float mTextZoom;      // Text zoom, defaults to 1.0

  PRInt16 mNumURLStarts;
  PRInt16 mDestroyRefCount;    // a second "refcount" for the document viewer's "destroy"

  unsigned      mEnableRendering : 1;
  unsigned      mStopped : 1;
  unsigned      mLoaded : 1;
  unsigned      mDeferredWindowClose : 1;
  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  unsigned      mIsSticky : 1;
  unsigned      mInPermitUnload : 1;

#ifdef NS_PRINTING
  unsigned      mClosingWhilePrinting : 1;

#if NS_PRINT_PREVIEW
  // These data members support delayed printing when the document is loading
  unsigned                         mPrintIsPending : 1;
  unsigned                         mPrintDocIsFullyLoaded : 1;
  nsCOMPtr<nsIPrintSettings>       mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;

  nsCOMPtr<nsPrintEngine>          mPrintEngine;
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
  
  PRPackedBool mIsPageMode;

};

//------------------------------------------------------------------
// DocumentViewerImpl
//------------------------------------------------------------------
// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kWidgetCID,            NS_CHILD_CID);

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
    mPrintEngine = nsnull;
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
DocumentViewerImpl::DocumentViewerImpl(nsPresContext* aPresContext)
  : mPresContext(aPresContext),
    mTextZoom(1.0),
    mIsSticky(PR_TRUE),
    mHintCharsetSource(kCharsetUninitialized)
{
  PrepareToStartLoad();
}

NS_IMPL_ADDREF(DocumentViewerImpl)
NS_IMPL_RELEASE(DocumentViewerImpl)

NS_INTERFACE_MAP_BEGIN(DocumentViewerImpl)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewer)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentViewer)
    NS_INTERFACE_MAP_ENTRY(nsIMarkupDocumentViewer)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerFile)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerEdit)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentViewerPrint)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentViewer)
#ifdef NS_PRINTING
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPrint)
#endif
NS_INTERFACE_MAP_END

DocumentViewerImpl::~DocumentViewerImpl()
{
  if (mDocument) {
    Close(nsnull);
    mDocument->Destroy();
  }

  NS_ASSERTION(!mPresShell && !mPresContext,
               "User did not call nsIContentViewer::Destroy");
  if (mPresShell || mPresContext) {
    // Make sure we don't hand out a reference to the content viewer to
    // the SHEntry!
    mSHEntry = nsnull;

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

  nsresult rv = NS_OK;
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
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryReferent(mContainer));
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
  mContainer = do_GetWeakReference(aContainer);
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

   *aResult = nsnull;
   nsCOMPtr<nsISupports> container = do_QueryReferent(mContainer);
   container.swap(*aResult);
   return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Init(nsIWidget* aParentWidget,
                         nsIDeviceContext* aDeviceContext,
                         const nsRect& aBounds)
{
  return InitInternal(aParentWidget, nsnull, aDeviceContext, aBounds, PR_TRUE, PR_FALSE);
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

  nscoord width = mPresContext->DevPixelsToAppUnits(bounds.width);
  nscoord height = mPresContext->DevPixelsToAppUnits(bounds.height);

  mViewManager->DisableRefresh();
  mViewManager->SetWindowDimensions(width, height);
  mPresContext->SetTextZoom(mTextZoom);

  // Setup default view manager background color

  // This may be overridden by the docshell with the background color
  // for the last document loaded into the docshell
  mViewManager->SetDefaultBackgroundColor(mPresContext->DefaultBackgroundColor());

  if (aDoInitialReflow) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLFrameSetElement> frameset =
        do_QueryInterface(mDocument->GetRootContent());
      htmlDoc->SetIsFrameset(frameset != nsnull);
    }

    // Initial reflow
    mPresShell->InitialReflow(width, height);

    // Now trigger a refresh
    if (mEnableRendering) {
      mViewManager->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
    }
  } else {
    // Store the visible area so it's available for other callers of
    // InitialReflow, like nsContentSink::StartLayout.
    mPresContext->SetVisibleArea(nsRect(0, 0, width, height));
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
  NS_ASSERTION(erP, "No event receiver in document!");

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
                                 nsISupports *aState,
                                 nsIDeviceContext* aDeviceContext,
                                 const nsRect& aBounds,
                                 PRBool aDoCreation,
                                 PRBool aInPrintPreview,
                                 PRBool aNeedMakeCX /*= PR_TRUE*/)
{
  mParentWidget = aParentWidget; // not ref counted

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  mDeviceContext = aDeviceContext;

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // Clear PrintPreview Alternate Device
  if (mDeviceContext) {
    mDeviceContext->SetAltDevice(nsnull);
  }
#endif

  PRBool makeCX = PR_FALSE;
  if (aDoCreation) {
    if (aParentWidget && !mPresContext) {
      // Create presentation context
      if (mIsPageMode) {
        //Presentation context already created in SetPageMode which is calling this method
      }
      else
        mPresContext =
            new nsPresContext(mDocument, nsPresContext::eContext_Galley);
      NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

      nsresult rv = mPresContext->Init(aDeviceContext); 
      if (NS_FAILED(rv)) {
        mPresContext = nsnull;
        return rv;
      }

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
      makeCX = !GetIsPrintPreview() && aNeedMakeCX; // needs to be true except when we are already in PP or we are enabling/disabling paginated mode.
#else
      makeCX = PR_TRUE;
#endif
    }

    if (mPresContext) {
      // Create the ViewManager and Root View...

      // We must do this before we tell the script global object about
      // this new document since doing that will cause us to re-enter
      // into nsSubDocumentFrame code through reflows caused by
      // FlushPendingNotifications() calls down the road...

      rv = MakeWindow(aParentWidget, aBounds);
      NS_ENSURE_SUCCESS(rv, rv);
      Hide();

#ifdef NS_PRINT_PREVIEW
      if (mIsPageMode) {
        nsCOMPtr<nsIDeviceContext> devctx;
        nsCOMPtr<nsIDeviceContextSpec> devspec =
          do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1");
        // XXX CRASHES ON OOM. YUM. WOULD SOMEONE PLEASE FIX ME.
        //     PERHAPS SOMEONE SHOULD HAVE REVIEWED THIS CODE.
        // XXX I have no idea how critical this code is, so i'm not fixing it.
        //     In fact I'm just adding a line that makes this block
        //     get compiled *less* often.
        // mWindow has been initialized by preceding call to MakeWindow
        devspec->Init(mWindow, mPresContext->GetPrintSettings(), PR_FALSE);
        // XXX CRASHES ON OOM under at least
        // nsPrintJobFactoryPS::CreatePrintJob. WOULD SOMEONE PLEASE FIX ME.
        //     PERHAPS SOMEONE SHOULD HAVE REVIEWED THIS CODE.
        // XXX I have no idea how critical this code is, so i'm not fixing it.
        //     In fact I'm just adding a line that makes this block
        //     get compiled *less* often.
        mDeviceContext->GetDeviceContextFor(devspec, *getter_AddRefs(devctx));
        mDeviceContext->SetAltDevice(devctx);
        mDeviceContext->SetUseAltDC(kUseAltDCFor_SURFACE_DIM, PR_TRUE);
        //Get paper dims:
        PRInt32 pageWidth, pageHeight;
        devctx->GetDeviceSurfaceDimensions(pageWidth, pageHeight);
        mPresContext->SetPageSize(nsSize(pageWidth, pageHeight));
        mPresContext->SetIsRootPaginatedDocument(PR_TRUE);
      }
#endif
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryReferent(mContainer));
  if (requestor) {
    if (mPresContext) {
      nsCOMPtr<nsILinkHandler> linkHandler;
      requestor->GetInterface(NS_GET_IID(nsILinkHandler),
                              getter_AddRefs(linkHandler));

      mPresContext->SetContainer(requestor);
      mPresContext->SetLinkHandler(linkHandler);
    }

    if (!aInPrintPreview) {
      // Set script-context-owner in the document

      nsCOMPtr<nsPIDOMWindow> window;
      requestor->GetInterface(NS_GET_IID(nsPIDOMWindow),
                              getter_AddRefs(window));

      if (window) {
        window->SetNewDocument(mDocument, aState, PR_TRUE);

        nsJSContext::LoadStart();
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

void
DocumentViewerImpl::DumpContentToPPM(const char* aFileName)
{
  mDocument->FlushPendingNotifications(Flush_Display);

  nsIScrollableView* scrollableView;
  mViewManager->GetRootScrollableView(&scrollableView);
  nsIView* view;
  if (scrollableView) {
    scrollableView->GetScrolledView(view);
  } else {
    mViewManager->GetRootView(view);
  }
  nsRect r = view->GetBounds() - view->GetPosition();
  // Limit the bitmap size to 5000x5000
  nscoord twipLimit = mPresContext->DevPixelsToAppUnits(5000);
  if (r.height > twipLimit)
    r.height = twipLimit;
  if (r.width > twipLimit)
    r.width = twipLimit;

  const char* status;

  if (r.IsEmpty()) {
    status = "EMPTY";
  } else {
    nsCOMPtr<nsIRenderingContext> context;
    nsresult rv = mPresShell->RenderOffscreen(r, PR_FALSE, PR_TRUE, 
                                              NS_RGB(255, 255, 255),
                                              getter_AddRefs(context));

    if (NS_FAILED(rv)) {
      status = "FAILEDRENDER";
    } else {
      nsIDrawingSurface* surface;
      context->GetDrawingSurface(&surface);
      if (!surface) {
        status = "NOSURFACE";
      } else {
        PRUint32 width = mPresContext->AppUnitsToDevPixels(view->GetBounds().width);
        PRUint32 height = mPresContext->AppUnitsToDevPixels(view->GetBounds().height);

        PRUint8* data;
        PRInt32 rowLen, rowSpan;
        rv = surface->Lock(0, 0, width, height, (void**)&data, &rowSpan, &rowLen,
                           NS_LOCK_SURFACE_READ_ONLY);
        if (NS_FAILED(rv)) {
          status = "FAILEDLOCK";
        } else {
          PRUint32 bytesPerPix = rowLen/width;
          nsPixelFormat format;
          surface->GetPixelFormat(&format);

          PRUint8* buf = new PRUint8[3*width];
          if (buf) {
            FILE* f = fopen(aFileName, "wb");
            if (!f) {
              status = "FOPENFAILED";
            } else {
              fprintf(f, "P6\n%d\n%d\n255\n", width, height);
              for (PRUint32 i = 0; i < height; ++i) {
                PRUint8* src = data + i*rowSpan;
                PRUint8* dest = buf;
                for (PRUint32 j = 0; j < width; ++j) {
                  /* v is the pixel value */
#ifdef IS_BIG_ENDIAN
                  PRUint32 v = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                  v >>= (32 - 8*bytesPerPix);
#else
                  PRUint32 v = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
#endif
                  dest[0] = ((v & format.mRedMask) >> format.mRedShift) << (8 - format.mRedCount);
                  dest[1] = ((v & format.mGreenMask) >> format.mGreenShift) << (8 - format.mGreenCount);
                  dest[2] = ((v & format.mBlueMask) >> format.mBlueShift) << (8 - format.mBlueCount);
                  src += bytesPerPix;
                  dest += 3;
                }
                fwrite(buf, 3, width, f);
              }
              fclose(f);
              status = "OK";
            }
            
            delete[] buf;
          }
          else {
            status = "OOM";
          }
          surface->Unlock();
        }
        context->DestroyDrawingSurface(surface);
      }
    }
  }

  nsIURI *uri = mDocument->GetDocumentURI();
  nsCAutoString spec;
  if (uri) {
    uri->GetAsciiSpec(spec);
  }
  printf("GECKO: PAINT FORCED AFTER ONLOAD: %s %s (%s)\n", spec.get(), aFileName, status);
  fflush(stdout);
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

  // First, get the window from the document...
  nsPIDOMWindow *window = mDocument->GetWindow();

  // Fail if no window is available...
  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);

  mLoaded = PR_TRUE;

  /* We need to protect ourself against auto-destruction in case the
     window is closed while processing the OnLoad event.  See bug
     http://bugzilla.mozilla.org/show_bug.cgi?id=78445 for more
     explanation.
  */
  nsCOMPtr<nsIDocumentViewer> kungFuDeathGrip(this);

  // Now, fire either an OnLoad or OnError event to the document...
  PRBool restoring = PR_FALSE;
  if(NS_SUCCEEDED(aStatus)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(PR_TRUE, NS_LOAD);
    event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
     // XXX Dispatching to |window|, but using |document| as the target.
    event.target = mDocument;

    // If the document presentation is being restored, we don't want to fire
    // onload to the document content since that would likely confuse scripts
    // on the page.

    nsIDocShell *docShell = window->GetDocShell();
    NS_ENSURE_TRUE(docShell, NS_ERROR_UNEXPECTED);

    docShell->GetRestoringDocument(&restoring);
    if (!restoring) {
      nsEventDispatcher::Dispatch(window, mPresContext, &event, nsnull,
                                  &status);
#ifdef MOZ_TIMELINE
      // if navigator.xul's load is complete, the main nav window is visible
      // mark that point.

      //printf("DEBUG: getting uri from document (%p)\n", mDocument.get());

      nsIURI *uri = mDocument ? mDocument->GetDocumentURI() : nsnull;

      if (uri) {
        //printf("DEBUG: getting spec for uri (%p)\n", uri.get());
        nsCAutoString spec;
        uri->GetSpec(spec);
        if (spec.EqualsLiteral("chrome://navigator/content/navigator.xul") ||
            spec.EqualsLiteral("chrome://browser/content/browser.xul")) {
          NS_TIMELINE_MARK("Navigator Window visible now");
        }
      }
#endif /* MOZ_TIMELINE */
    }
  } else {
    // XXX: Should fire error event to the document...
  }

  // Notify the document that it has been shown (regardless of whether
  // it was just loaded). Note: mDocument may be null now if the above
  // firing of onload caused the document to unload.
  if (mDocument)
    mDocument->OnPageShow(restoring);

  // Now that the document has loaded, we can tell the presshell
  // to unsuppress painting.
  if (mPresShell && !mStopped) {
    mPresShell->UnsuppressPainting();
  }

  static PRBool forcePaint
    = PR_GetEnv("MOZ_FORCE_PAINT_AFTER_ONLOAD") != nsnull;
  static PRUint32 index = 0;
  if (forcePaint) {
    nsCAutoString name(PR_GetEnv("MOZ_FORCE_PAINT_AFTER_ONLOAD"));
    name.AppendLiteral("-");
    ++index;
    name.AppendInt(index);
    DumpContentToPPM(name.get());
  }

  nsJSContext::LoadEnd();

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
  nsPIDOMWindow *window = mDocument->GetWindow();

  if (!window) {
    // This is odd, but not fatal
    NS_WARNING("window not set for document!");
    return NS_OK;
  }

  // Now, fire an BeforeUnload event to the document and see if it's ok
  // to unload...
  nsEventStatus status = nsEventStatus_eIgnore;
  nsBeforePageUnloadEvent event(PR_TRUE, NS_BEFORE_PAGE_UNLOAD);
  event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
  // XXX Dispatching to |window|, but using |document| as the target.
  event.target = mDocument;
  nsresult rv = NS_OK;

  // In evil cases we might be destroyed while handling the
  // onbeforeunload event, don't let that happen. (see also bug#331040)
  nsRefPtr<DocumentViewerImpl> kungFuDeathGrip(this);

  {
    // Never permit popups from the beforeunload handler, no matter
    // how we get here.
    nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

    mInPermitUnload = PR_TRUE;
    nsEventDispatcher::Dispatch(window, mPresContext, &event, nsnull, &status);
    mInPermitUnload = PR_FALSE;
  }

  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryReferent(mContainer));

  if (NS_SUCCEEDED(rv) && (event.flags & NS_EVENT_FLAG_NO_DEFAULT ||
                           !event.text.IsEmpty())) {
    // Ask the user if it's ok to unload the current page

    nsCOMPtr<nsIPrompt> prompt = do_GetInterface(docShellNode);

    if (prompt) {
      nsXPIDLString preMsg, postMsg;
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              "OnBeforeUnloadPreMessage",
                                              preMsg);
      rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadPostMessage",
                                               postMsg);

      // GetStringFromName can succeed, yet give NULL strings back.
      if (NS_FAILED(rv) || preMsg.IsEmpty() || postMsg.IsEmpty()) {
        NS_ERROR("Failed to get strings from dom.properties!");
        return NS_OK;
      }

      // Limit the length of the text the page can inject into this
      // dialogue to 1024 characters.
      PRInt32 len = PR_MIN(event.text.Length(), 1024);

      nsAutoString msg;
      if (len == 0) {
        msg = preMsg + NS_LITERAL_STRING("\n\n") + postMsg;
      } else {
        msg = preMsg + NS_LITERAL_STRING("\n\n") +
              StringHead(event.text, len) +
              NS_LITERAL_STRING("\n\n") + postMsg;
      } 

      // This doesn't pass a title, which makes the title be
      // "Confirm", is that ok, or do we want a localizable title for
      // this dialogue?
      if (NS_FAILED(prompt->Confirm(nsnull, msg.get(), aPermitUnload))) {
        *aPermitUnload = PR_TRUE;
      }
    }
  }

  if (docShellNode) {
    PRInt32 childCount;
    docShellNode->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount && *aPermitUnload; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      docShellNode->GetChildAt(i, getter_AddRefs(item));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(item));

      if (docShell) {
        nsCOMPtr<nsIContentViewer> cv;
      docShell->GetContentViewer(getter_AddRefs(cv));

      if (cv) {
        cv->PermitUnload(aPermitUnload);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::PageHide(PRBool aIsUnload)
{
  mEnableRendering = PR_FALSE;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument->OnPageHide(!aIsUnload);
  if (aIsUnload) {
    // if Destroy() was called during OnPageHide(), mDocument is nsnull.
    NS_ENSURE_STATE(mDocument);

    // First, get the window from the document...
    nsPIDOMWindow *window = mDocument->GetWindow();

    if (!window) {
      // Fail if no window is available...
      NS_ERROR("window not set for document!");
      return NS_ERROR_NULL_POINTER;
    }

    // Now, fire an Unload event to the document...
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(PR_TRUE, NS_PAGE_UNLOAD);
    event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
    // XXX Dispatching to |window|, but using |document| as the target.
    event.target = mDocument;

    // Never permit popups from the unload handler, no matter how we get
    // here.
    nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

    nsEventDispatcher::Dispatch(window, mPresContext, &event, nsnull, &status);
  }

  if (mPresShell) {
    // look for open menupopups and close them after the unload event, in case
    // the unload event listeners open any new popups
    nsIViewManager *vm = mPresShell->GetViewManager();
    if (vm) {
      nsIView *rootView = nsnull;
      vm->GetRootView(rootView);
      if (rootView)
        HideViewIfPopup(rootView);
    }
  }

  return NS_OK;
}

void
DocumentViewerImpl::HideViewIfPopup(nsIView* aView)
{
  nsIFrame* frame = NS_STATIC_CAST(nsIFrame*, aView->GetClientData());
  if (frame) {
    nsIMenuParent* parent;
    CallQueryInterface(frame, &parent);
    if (parent) {
      parent->HideChain();
      // really make sure the view is hidden
      mViewManager->SetViewVisibility(aView, nsViewVisibility_kHide);
    }
  }

  nsIView* child = aView->GetFirstChild();
  while (child) {
    HideViewIfPopup(child);
    child = child->GetNextSibling();
  }
}

static void
AttachContainerRecurse(nsIDocShell* aShell)
{
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(viewer);
  if (docViewer) {
    nsCOMPtr<nsIDocument> doc;
    docViewer->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->SetContainer(aShell);
    }
    nsCOMPtr<nsPresContext> pc;
    docViewer->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      pc->SetContainer(aShell);
      pc->SetLinkHandler(nsCOMPtr<nsILinkHandler>(do_QueryInterface(aShell)));
    }
    nsCOMPtr<nsIPresShell> presShell;
    docViewer->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->SetForwardingContainer(nsnull);
    }
  }

  // Now recurse through the children
  nsCOMPtr<nsIDocShellTreeNode> node = do_QueryInterface(aShell);
  NS_ASSERTION(node, "docshells must implement nsIDocShellTreeNode");

  PRInt32 childCount;
  node->GetChildCount(&childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    node->GetChildAt(i, getter_AddRefs(childItem));
    AttachContainerRecurse(nsCOMPtr<nsIDocShell>(do_QueryInterface(childItem)));
  }
}

NS_IMETHODIMP
DocumentViewerImpl::Open(nsISupports *aState, nsISHEntry *aSHEntry)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);

  // Our container might have gone away while we were closed.
  // If this is the case, we must fail to open so we don't crash.
  nsCOMPtr<nsISupports> container = do_QueryReferent(mContainer);
  if (!container)
    return NS_ERROR_NOT_AVAILABLE;

  nsRect bounds;
  mWindow->GetBounds(bounds);

  nsresult rv = InitInternal(mParentWidget, aState, mDeviceContext, bounds,
                             PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDocument)
    mDocument->SetContainer(nsCOMPtr<nsISupports>(do_QueryReferent(mContainer)));

  if (mPresShell)
    mPresShell->SetForwardingContainer(nsnull);

  // Rehook the child presentations.  The child shells are still in
  // session history, so get them from there.

  nsCOMPtr<nsIDocShellTreeItem> item;
  PRInt32 itemIndex = 0;
  while (NS_SUCCEEDED(aSHEntry->ChildShellAt(itemIndex++,
                                             getter_AddRefs(item))) && item) {
    AttachContainerRecurse(nsCOMPtr<nsIDocShell>(do_QueryInterface(item)));
  }
  
  SyncParentSubDocMap();

  if (mFocusListener) {
    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
    NS_ASSERTION(erP, "No event receiver in document!");

    if (erP) {
      erP->AddEventListenerByIID(mFocusListener,
                                 NS_GET_IID(nsIDOMFocusListener));
    }
  }

  // XXX re-enable image animations once that works correctly

  PrepareToStartLoad();
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Close(nsISHEntry *aSHEntry)
{
  // All callers are supposed to call close to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

  mSHEntry = aSHEntry;

  // Close is also needed to disable scripts during paint suppression,
  // since we transfer the existing global object to the new document
  // that is loaded.  In the future, the global object may become a proxy
  // for an object that can be switched in and out so that we don't need
  // to disable scripts during paint suppression.

  if (!mDocument)
    return NS_OK;

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // Turn scripting back on
  // after PrintPreview had turned it off
  if (GetIsPrintPreview() && mPrintEngine) {
    mPrintEngine->TurnScriptingOn(PR_TRUE);
  }
#endif

#ifdef NS_PRINTING
  // A Close was called while we were printing
  // so don't clear the ScriptGlobalObject
  // or clear the mDocument below
  // Also, do an extra addref to keep the viewer from going away.
  if (mPrintEngine && !mClosingWhilePrinting) {
    mClosingWhilePrinting = PR_TRUE;
    NS_ADDREF_THIS();
  } else
#endif
    {
      // out of band cleanup of webshell
      mDocument->SetScriptGlobalObject(nsnull);

      if (!mSHEntry)
        mDocument->Destroy();
    }

  if (mFocusListener) {
    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> erP(do_QueryInterface(mDocument));
    NS_ASSERTION(erP, "No event receiver in document!");

    if (erP) {
      erP->RemoveEventListenerByIID(mFocusListener,
                                    NS_GET_IID(nsIDOMFocusListener));
    }
  }

  return NS_OK;
}

static void
DetachContainerRecurse(nsIDocShell *aShell)
{
  // Unhook this docshell's presentation
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(viewer);
  if (docViewer) {
    nsCOMPtr<nsIDocument> doc;
    docViewer->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->SetContainer(nsnull);
    }
    nsCOMPtr<nsPresContext> pc;
    docViewer->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      pc->SetContainer(nsnull);
      pc->SetLinkHandler(nsnull);
    }
    nsCOMPtr<nsIPresShell> presShell;
    docViewer->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->SetForwardingContainer(nsWeakPtr(do_GetWeakReference(aShell)));
    }
  }

  // Now recurse through the children
  nsCOMPtr<nsIDocShellTreeNode> node = do_QueryInterface(aShell);
  NS_ASSERTION(node, "docshells must implement nsIDocShellTreeNode");

  PRInt32 childCount;
  node->GetChildCount(&childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    node->GetChildAt(i, getter_AddRefs(childItem));
    DetachContainerRecurse(nsCOMPtr<nsIDocShell>(do_QueryInterface(childItem)));
  }
}

NS_IMETHODIMP
DocumentViewerImpl::Destroy()
{
  NS_ASSERTION(mDocument, "No document in Destroy()!");

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

  // Don't let the document get unloaded while we are printing.
  // this could happen if we hit the back button during printing.
  // We also keep the viewer from being cached in session history, since
  // we require all documents there to be sanitized.
  if (mDestroyRefCount != 0) {
    --mDestroyRefCount;
    return NS_OK;
  }

  // If we were told to put ourselves into session history instead of destroy
  // the presentation, do that now.
  if (mSHEntry) {
    if (mPresShell)
      mPresShell->Freeze();

    // Make sure the presentation isn't torn down by Hide().
    mSHEntry->SetSticky(mIsSticky);
    mIsSticky = PR_TRUE;

    PRBool savePresentation = PR_TRUE;

    // Remove our root view from the view hierarchy.
    if (mPresShell) {
      nsIViewManager *vm = mPresShell->GetViewManager();
      if (vm) {
        nsIView *rootView = nsnull;
        vm->GetRootView(rootView);

        if (rootView) {
          nsIView *rootViewParent = rootView->GetParent();
          if (rootViewParent) {
            nsIViewManager *parentVM = rootViewParent->GetViewManager();
            if (parentVM) {
              parentVM->RemoveChild(rootView);
            }
          }
        }
      }
    }

    Hide();

    // This is after Hide() so that the user doesn't see the inputs clear.
    if (mDocument) {
      nsresult rv = mDocument->Sanitize();
      if (NS_FAILED(rv)) {
        // If we failed to sanitize, don't save presentation.
        savePresentation = PR_FALSE;
      }
    }


    // Reverse ownership. Do this *after* calling sanitize so that sanitize
    // doesn't cause mutations that make the SHEntry drop the presentation
    if (savePresentation) {
      mSHEntry->SetContentViewer(this);
    }
    else {
      mSHEntry->SyncPresentationState();
    }
    nsCOMPtr<nsISHEntry> shEntry = mSHEntry; // we'll need this below
    mSHEntry = nsnull;

    // Break the link from the document/presentation to the docshell, so that
    // link traversals cannot affect the currently-loaded document.
    // When the presentation is restored, Open() and InitInternal() will reset
    // these pointers to their original values.

    if (mDocument)
      mDocument->SetContainer(nsnull);
    if (mPresContext) {
      mPresContext->SetLinkHandler(nsnull);
      mPresContext->SetContainer(nsnull);
    }
    if (mPresShell)
      mPresShell->SetForwardingContainer(mContainer);

    // Do the same for our children.  Note that we need to get the child
    // docshells from the SHEntry now; the docshell will have cleared them.
    nsCOMPtr<nsIDocShellTreeItem> item;
    PRInt32 itemIndex = 0;
    while (NS_SUCCEEDED(shEntry->ChildShellAt(itemIndex++,
                                              getter_AddRefs(item))) && item) {
      DetachContainerRecurse(nsCOMPtr<nsIDocShell>(do_QueryInterface(item)));
    }

    return NS_OK;
  }

  if (mDocument) {
    mDocument->Destroy();
    mDocument = nsnull;
  }

  // All callers are supposed to call destroy to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

#ifdef NS_PRINTING
  if (mPrintEngine) {
    mPrintEngine->Destroy();
    mPrintEngine = nsnull;
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

  if (mEnableRendering && (mLoaded || mStopped) && mPresContext && !mSHEntry)
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

  // Set new container
  nsCOMPtr<nsISupports> container = do_QueryReferent(mContainer);
  newDoc->SetContainer(container);

  if (mDocument != newDoc) {
    // Replace the old document with the new one. Do this only when
    // the new document really is a new document.
    mDocument = newDoc;

    // Set the script global object on the new document
    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(container);
    if (window) {
      window->SetNewDocument(newDoc, nsnull, PR_TRUE);
    }

    // Clear the list of old child docshells. CChild docshells for the new
    // document will be constructed as frames are created.
    nsCOMPtr<nsIDocShellTreeNode> node = do_QueryInterface(container);
    if (node) {
      PRInt32 count;
      node->GetChildCount(&count);
      for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        node->GetChildAt(0, getter_AddRefs(child));
        node->RemoveChild(child);
      }
    }
  }

  rv = SyncParentSubDocMap();
  NS_ENSURE_SUCCESS(rv, rv);

  // Replace the current pres shell with a new shell for the new document

  nsCOMPtr<nsILinkHandler> linkHandler;
  if (mPresShell) {
    if (mPresContext) {
      // Save the linkhandler (nsPresShell::Destroy removes it from
      // mPresContext).
      linkHandler = mPresContext->GetLinkHandler();
    }

    mPresShell->EndObservingDocument();
    mPresShell->Destroy();

    mPresShell = nsnull;
  }

  // And if we're already given a prescontext...
  if (mPresContext) {
    // If we had a linkHandler and it got removed, put it back.
    if (linkHandler) {
      mPresContext->SetLinkHandler(linkHandler);
    }

    // Create a new style set for the document

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

    // Register the focus listener on the new document

    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(mDocument, &rv);
    NS_ASSERTION(erP, "No event receiver in document!");

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
DocumentViewerImpl::GetPresContext(nsPresContext** aResult)
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

  // If there's a previous viewer, it's the one that's actually showing,
  // so be sure to resize it as well so it paints over the right area.
  // This may slow down the performance of the new page load, but resize
  // during load is also probably a relatively unusual condition
  // relating to things being hidden while something is loaded.  It so
  // happens that Firefox does this a good bit with its infobar, and it
  // looks ugly if we don't do this.
  if (mPreviousViewer)
    mPreviousViewer->SetBounds(aBounds);

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (GetIsPrintPreview() && !mPrintEngine->GetIsCreatingPrintPreview()) {
    mPrintEngine->GetPrintPreviewWindow()->Resize(aBounds.x, aBounds.y,
                                                  aBounds.width, aBounds.height,
                                                  PR_FALSE);
  }
#endif
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

    // Make sure we don't have too many cached ContentViewers
    nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryReferent(mContainer);
    if (treeItem) {
      // We need to find the root DocShell since only that object has an
      // SHistory and we need the SHistory to evict content viewers
      nsCOMPtr<nsIDocShellTreeItem> root;
      treeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
      nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(root);
      nsCOMPtr<nsISHistory> history;
      webNav->GetSessionHistory(getter_AddRefs(history));
      nsCOMPtr<nsISHistoryInternal> historyInt = do_QueryInterface(history);
      if (historyInt) {
        PRInt32 prevIndex,loadedIndex;
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(treeItem);
        docShell->GetPreviousTransIndex(&prevIndex);
        docShell->GetLoadedTransIndex(&loadedIndex);
#ifdef DEBUG_PAGE_CACHE
        printf("About to evict content viewers: prev=%d, loaded=%d\n",
               prevIndex, loadedIndex);
#endif
        historyInt->EvictContentViewers(prevIndex, loadedIndex);
      }
    }
  }

  if (mWindow) {
    mWindow->Show(PR_TRUE);
  }

  if (mDocument && !mPresShell && !mWindow) {
    nsresult rv;

    nsCOMPtr<nsIBaseWindow> base_win(do_QueryReferent(mContainer));
    NS_ENSURE_TRUE(base_win, NS_ERROR_UNEXPECTED);

    base_win->GetParentWidget(&mParentWidget);
    NS_ENSURE_TRUE(mParentWidget, NS_ERROR_UNEXPECTED);

    mDeviceContext = mParentWidget->GetDeviceContext();

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
    // Clear PrintPreview Alternate Device
    if (mDeviceContext) {
      mDeviceContext->SetAltDevice(nsnull);
    }
#endif

    // Create presentation context
    NS_ASSERTION(!mPresContext, "Shouldn't have a prescontext if we have no shell!");
    mPresContext = new nsPresContext(mDocument, nsPresContext::eContext_Galley);
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

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

    if (mPresContext && base_win) {
      nsCOMPtr<nsILinkHandler> linkHandler(do_GetInterface(base_win));

      if (linkHandler) {
        mPresContext->SetLinkHandler(linkHandler);
      }

      mPresContext->SetContainer(base_win);
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
  NS_PRECONDITION(mWindow, "null window");
  if (mWindow) {
    mWindow->Show(PR_FALSE);
  }

  if (!mPresShell)
    return NS_OK;

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

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
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

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryReferent(mContainer));

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

PR_STATIC_CALLBACK(PRBool)
AppendAgentSheet(nsIStyleSheet *aSheet, void *aData)
{
  nsStyleSet *styleSet = NS_STATIC_CAST(nsStyleSet*, aData);
  styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, aSheet);
  return PR_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
PrependUserSheet(nsIStyleSheet *aSheet, void *aData)
{
  nsStyleSet *styleSet = NS_STATIC_CAST(nsStyleSet*, aData);
  styleSet->PrependStyleSheet(nsStyleSet::eUserSheet, aSheet);
  return PR_TRUE;
}

nsresult
DocumentViewerImpl::CreateStyleSet(nsIDocument* aDocument,
                                   nsStyleSet** aStyleSet)
{
  // Make sure this does the same thing as PresShell::AddSheet wrt ordering.

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
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryReferent(mContainer));
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
    styleSet->AppendStyleSheet(nsStyleSet::eUserSheet, sheet);

  // Append chrome sheets (scrollbars + forms).
  PRBool shouldOverride = PR_FALSE;
  nsCOMPtr<nsIDocShell> ds(do_QueryInterface(docShell));
  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
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
        nsCOMPtr<nsICSSLoader> cssLoader;
        NS_NewCSSLoader(getter_AddRefs(cssLoader));

        char *str = ToNewCString(sheets);
        char *newStr = str;
        char *token;
        while ( (token = nsCRT::strtok(newStr, ", ", &newStr)) ) {
          NS_NewURI(getter_AddRefs(uri), nsDependentCString(token), nsnull,
                    baseURI);
          if (!uri) continue;

          cssLoader->LoadSheetSync(uri, getter_AddRefs(csssheet));
          if (!sheet) continue;

          styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, csssheet);
          shouldOverride = PR_TRUE;
        }
        nsMemory::Free(str);
      }
    }
  }

  if (!shouldOverride) {
    sheet = nsLayoutStylesheetCache::ScrollbarsSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, sheet);
    }
  }

  sheet = nsLayoutStylesheetCache::FormsSheet();
  if (sheet) {
    styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, sheet);
  }

  if (mUAStyleSheet) {
    styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, mUAStyleSheet);
  }

  nsCOMPtr<nsIStyleSheetService> dummy =
    do_GetService(NS_STYLESHEETSERVICE_CONTRACTID);

  nsStyleSheetService *sheetService = nsStyleSheetService::gInstance;
  if (sheetService) {
    sheetService->AgentStyleSheets()->EnumerateForwards(AppendAgentSheet,
                                                        styleSet);
    sheetService->UserStyleSheets()->EnumerateBackwards(PrependUserSheet,
                                                        styleSet);
  }

  // Caller will handle calling EndUpdate, per contract.
  *aStyleSet = styleSet;
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::ClearHistoryEntry()
{
  mSHEntry = nsnull;
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
  tbounds *= mPresContext->AppUnitsPerDevPixel();

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
  // if aParentWidget has a view, we'll hook our view manager up to its view tree
  nsIView* containerView = nsView::GetViewFor(aParentWidget);

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
      nsCOMPtr<nsIDocShellTreeItem> container(do_QueryReferent(mContainer));
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

  // Create a view
  nsIView* view = mViewManager->CreateView(tbounds, containerView);
  if (!view)
    return NS_ERROR_OUT_OF_MEMORY;

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

// Return the selection for the document. Note that text fields have their
// own selection, which cannot be accessed with this method. Use
// mPresShell->GetSelectionForCopy() instead.
nsresult DocumentViewerImpl::GetDocumentSelection(nsISelection **aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);
  if (!mPresShell) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsISelectionController> selcon;
  selcon = do_QueryInterface(mPresShell);
  if (selcon)
    return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                aSelection);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocumentViewerImpl::CreateDocumentViewerUsing(nsPresContext* aPresContext,
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
  // Nothing to do here.
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetSearchable(PRBool *aSearchable)
{
  // Nothing to do here.
  *aSearchable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::ClearSelection()
{
  nsresult rv;
  nsCOMPtr<nsISelection> selection;

  // use mPresShell->GetSelectionForCopy() ?
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

  // use mPresShell->GetSelectionForCopy() ?
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

NS_IMETHODIMP DocumentViewerImpl::CopyImage(PRInt32 aCopyFlags)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIImageLoadingContent> node;
  GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  return nsCopySupport::ImageCopy(node, aCopyFlags);
}

NS_IMETHODIMP DocumentViewerImpl::GetCopyable(PRBool *aCopyable)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelection> selection;
  nsresult rv = mPresShell->GetSelectionForCopy(getter_AddRefs(selection));
  if (NS_FAILED(rv)) return rv;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  *aCopyable = !isCollapsed;
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::CutSelection()
{
  // Nothing to do here.
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetCutable(PRBool *aCutable)
{
  *aCutable = PR_FALSE;  // mm, will this ever be called for an editable document?
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::Paste()
{
  // Nothing to do here.
  return NS_OK;
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
  return GetCopyable(aCanGetContents);
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
#ifdef NS_PRINTING
  return Print(aThePrintSettings, aWPListener);
#else
  return NS_ERROR_FAILURE;
#endif
}

// nsIContentViewerFile interface
NS_IMETHODIMP
DocumentViewerImpl::GetPrintable(PRBool *aPrintable)
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = !GetIsPrinting();

  return NS_OK;
}

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
   // Remember Frames aren't ref-counted.  They are in their own special little
   // world.
   nsIFrame* frame = presShell->GetPrimaryFrameFor(content);

   NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

   // tell the pres shell to scroll to the frame
   NS_ENSURE_SUCCESS(presShell->ScrollFrameIntoView(frame,
                                                    NS_PRESSHELL_SCROLL_TOP,
                                                    NS_PRESSHELL_SCROLL_ANYWHERE),
                     NS_ERROR_FAILURE);
   return NS_OK;
}

void
DocumentViewerImpl::CallChildren(CallChildFunc aFunc, void* aClosure)
{
  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryReferent(mContainer));
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
  mTextZoom = aTextZoom;

  if (mViewManager) {
    mViewManager->BeginUpdateViewBatch();
  }
      
  // Set the text zoom on all children of mContainer (even if our zoom didn't
  // change, our children's zoom may be different, though it would be unusual).
  // Do this first, in case kids are auto-sizing and post reflow commands on
  // our presshell (which should be subsumed into our own style change reflow).
  struct TextZoomInfo textZoomInfo = { aTextZoom };
  CallChildren(SetChildTextZoom, &textZoomInfo);

  // Now change our own zoom
  if (mPresContext && aTextZoom != mPresContext->TextZoom()) {
      mPresContext->SetTextZoom(aTextZoom);
  }

  if (mViewManager) {
    mViewManager->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);
  NS_ASSERTION(!mPresContext || mPresContext->TextZoom() == mTextZoom, 
               "mPresContext->TextZoom() != mTextZoom");

  *aTextZoom = mTextZoom;
  return NS_OK;
}

static void
SetChildAuthorStyleDisabled(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  PRBool styleDisabled  = *NS_STATIC_CAST(PRBool*, aClosure);
  aChild->SetAuthorStyleDisabled(styleDisabled);
}


NS_IMETHODIMP
DocumentViewerImpl::SetAuthorStyleDisabled(PRBool aStyleDisabled)
{
  if (mPresShell) {
    mPresShell->SetAuthorStyleDisabled(aStyleDisabled);
  }
  CallChildren(SetChildAuthorStyleDisabled, &aStyleDisabled);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetAuthorStyleDisabled(PRBool* aStyleDisabled)
{
  if (mPresShell) {
    *aStyleDisabled = mPresShell->GetAuthorStyleDisabled();
  } else {
    *aStyleDisabled = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetDefaultCharacterSet(nsACString& aDefaultCharacterSet)
{
  NS_ENSURE_STATE(nsCOMPtr<nsISupports>(do_QueryReferent(mContainer)));

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
  CallChildren(SetChildDefaultCharacterSet, (void*) &aDefaultCharacterSet);
  return NS_OK;
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
  CallChildren(SetChildForceCharacterSet, (void*) &aForceCharacterSet);
  return NS_OK;
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
  CallChildren(SetChildPrevDocCharacterSet, (void*) &aPrevDocCharacterSet);
  return NS_OK;
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
  CallChildren(SetChildHintCharacterSetSource,
                      (void*) aHintCharacterSetSource);
  return NS_OK;
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
  CallChildren(SetChildHintCharacterSet, (void*) &aHintCharacterSet);
  return NS_OK;
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
    mPresContext->SetBidi(aBidiOptions, PR_TRUE); // could cause reflow
  }
  // now set bidi on all children of mContainer
  CallChildren(SetChildBidiOptions, (void*) aBidiOptions);
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::GetBidiOptions(PRUint32* aBidiOptions)
{
  if (aBidiOptions) {
    if (mPresContext) {
      *aBidiOptions = mPresContext->GetBidi();
    }
    else
      *aBidiOptions = IBMBIDI_DEFAULT_BIDI_OPTIONS;
  }
  return NS_OK;
}

NS_IMETHODIMP DocumentViewerImpl::SizeToContent()
{
   NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryReferent(mContainer));
   NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShellTreeItem> docShellParent;
   docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

   // It's only valid to access this from a top frame.  Doesn't work from
   // sub-frames.
   NS_ENSURE_TRUE(!docShellParent, NS_ERROR_FAILURE);

   nsCOMPtr<nsIPresShell> presShell;
   GetPresShell(getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   // Flush out all content and style updates. We can't use a resize reflow
   // because it won't change some sizes that a style change reflow will.
   mDocument->FlushPendingNotifications(Flush_Layout);

  nsIFrame *root = presShell->GetRootFrame();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  nscoord prefWidth;
  {
    nsCOMPtr<nsIRenderingContext> rcx;
    presShell->CreateRenderingContext(root, getter_AddRefs(rcx));
    NS_ENSURE_TRUE(rcx, NS_ERROR_FAILURE);
    prefWidth = root->GetPrefWidth(rcx);
  }

  nsresult rv = presShell->ResizeReflow(prefWidth, NS_UNCONSTRAINEDSIZE);
  NS_ENSURE_SUCCESS(rv, rv);

   nsCOMPtr<nsPresContext> presContext;
   GetPresContext(getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   PRInt32 width, height;

   // so how big is it?
   nsRect shellArea = presContext->GetVisibleArea();
   if (shellArea.width == NS_UNCONSTRAINEDSIZE ||
       shellArea.height == NS_UNCONSTRAINEDSIZE) {
     // Protect against bogus returns here
     return NS_ERROR_FAILURE;
   }
   width = presContext->AppUnitsToDevPixels(shellArea.width);
   height = presContext->AppUnitsToDevPixels(shellArea.height);

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
  nsPIDOMWindow *privateWin = document->GetWindow();
  NS_ENSURE_TRUE(privateWin, NS_ERROR_NOT_AVAILABLE);

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

  if (node)
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

NS_IMETHODIMP nsDocViewerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, PRInt16)
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

    nsPIDOMWindow *domWindow = theDoc->GetWindow();
    if (!domWindow) return NS_ERROR_FAILURE;

    domWindow->UpdateCommands(NS_LITERAL_STRING("select"));
    mGotSelectionState = PR_TRUE;
    mSelectionWasCollapsed = selectionCollapsed;
  }

  return NS_OK;
}

//nsDocViewerFocusListener
NS_IMPL_ISUPPORTS2(nsDocViewerFocusListener,
                   nsIDOMFocusListener,
                   nsIDOMEventListener)

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
  selCon->GetDisplaySelection(&selectionStatus);

  // If selection was disabled, re-enable it.
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

  // If selection was on, disable it.
  if(selectionStatus == nsISelectionController::SELECTION_ON ||
     selectionStatus == nsISelectionController::SELECTION_ATTENTION)
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

#ifdef NS_PRINTING

NS_IMETHODIMP
DocumentViewerImpl::Print(nsIPrintSettings*       aPrintSettings,
                          nsIWebProgressListener* aWebProgressListener)
{

#ifdef MOZ_XUL
  // Temporary code for Bug 136185 / Bug 240490
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    nsPrintEngine::ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_NO_XUL);
    return NS_ERROR_FAILURE;
  }
#endif

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
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
  if (!presShell || !mDocument || !mDeviceContext || !mParentWidget) {
    PR_PL(("Can't Print without pres shell, document etc"));
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

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

    rv = mPrintEngine->Initialize(this, docShell, mDocument, 
                                  mDeviceContext, mParentWidget,
#ifdef NS_DEBUG
                                  mDebugFile
#else
                                  nsnull
#endif
                                  );
    if (NS_FAILED(rv)) {
      mPrintEngine->Destroy();
      mPrintEngine = nsnull;
      return rv;
    }
  }

  rv = mPrintEngine->Print(aPrintSettings, aWebProgressListener);
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
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

#ifdef MOZ_XUL
  // Temporary code for Bug 136185 / Bug 240490
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    nsPrintEngine::CloseProgressDialog(aWebProgressListener);
    nsPrintEngine::ShowPrintErrorDialog(NS_ERROR_GFX_PRINTER_NO_XUL, PR_FALSE);
    return NS_ERROR_FAILURE;
  }
#endif

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  NS_ASSERTION(docShell, "This has to be a docshell");
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell || !mDocument || !mDeviceContext || !mParentWidget) {
    PR_PL(("Can't Print Preview without pres shell, document etc"));
    return NS_ERROR_FAILURE;
  }

  if (!mPrintEngine) {
    mPrintEngine = new nsPrintEngine();
    NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_OUT_OF_MEMORY);

    rv = mPrintEngine->Initialize(this, docShell, mDocument,
                                  mDeviceContext, mParentWidget,
#ifdef NS_DEBUG
                                  mDebugFile
#else
                                  nsnull
#endif
                                  );
    if (NS_FAILED(rv)) {
      mPrintEngine->Destroy();
      mPrintEngine = nsnull;
      return rv;
    }
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
  if (!GetIsPrintPreview() ||
      mPrintEngine->GetIsCreatingPrintPreview())
    return NS_ERROR_FAILURE;

  nsIScrollableView* scrollableView = nsnull;
  mPrintEngine->GetPrintPreviewViewManager()->GetRootScrollableView(&scrollableView);
  if (scrollableView == nsnull)
    return NS_OK;

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
      fndPageFrame = currentPage->GetPrevInFlow();
      if (!fndPageFrame) {
        return NS_OK;
      }
    } else {
      return NS_OK;
    }
  } else if (aType == nsIWebBrowserPrint::PRINTPREVIEW_NEXT_PAGE) {
    if (currentPage) {
      fndPageFrame = currentPage->GetNextInFlow();
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
    nscoord deadSpaceGap = 0;
    nsIPageSequenceFrame * sqf;
    if (NS_SUCCEEDED(CallQueryInterface(seqFrame, &sqf))) {
      sqf->GetDeadSpaceValue(&deadSpaceGap);
    }

    // scroll so that top of page (plus the gray area) is at the top of the scroll area
    scrollableView->ScrollTo(0, fndPageFrame->GetPosition().y-deadSpaceGap, PR_TRUE);
  }
  return NS_OK;

}

/* readonly attribute nsIPrintSettings globalPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetGlobalPrintSettings(nsIPrintSettings * *aGlobalPrintSettings)
{
  return nsPrintEngine::GetGlobalPrintSettings(aGlobalPrintSettings);
}

/* readonly attribute boolean doingPrint; */
// XXX This always returns PR_FALSE for subdocuments
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrint(PRBool *aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  
  *aDoingPrint = PR_FALSE;
  if (mPrintEngine) {
    // XXX shouldn't this be GetDoingPrint() ?
    return mPrintEngine->GetDoingPrintPreview(aDoingPrint);
  } 
  return NS_OK;
}

/* readonly attribute boolean doingPrintPreview; */
// XXX This always returns PR_FALSE for subdocuments
NS_IMETHODIMP
DocumentViewerImpl::GetDoingPrintPreview(PRBool *aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);

  *aDoingPrintPreview = PR_FALSE;
  if (mPrintEngine) {
    return mPrintEngine->GetDoingPrintPreview(aDoingPrintPreview);
  }
  return NS_OK;
}

/* readonly attribute nsIPrintSettings currentPrintSettings; */
NS_IMETHODIMP
DocumentViewerImpl::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  *aCurrentPrintSettings = nsnull;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetCurrentPrintSettings(aCurrentPrintSettings);
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
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);
  return mPrintEngine->Cancelled();
}

/* void exitPrintPreview (); */
NS_IMETHODIMP
DocumentViewerImpl::ExitPrintPreview()
{
  if (GetIsPrinting()) return NS_ERROR_FAILURE;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  if (GetIsPrintPreview()) {
    ReturnToGalleyPresentation();
  }
  return NS_OK;
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
// XXX this always returns PR_FALSE for subdocuments
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
    nsCOMPtr<nsIDocShellTreeNode> docShellTreeNode(do_QueryReferent(mContainer));
    NS_ASSERTION(docShellTreeNode, "mContainer has to be a nsIDocShellTreeNode");
    SetIsPrintingInDocShellTree(docShellTreeNode, aIsPrinting, PR_TRUE);
  }
#endif
}

//------------------------------------------------------------
// The PrintEngine holds the current value
// this called from inside the DocViewer.
// XXX it always returns PR_FALSE for subdocuments
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
    nsCOMPtr<nsIDocShellTreeNode> docShellTreeNode(do_QueryReferent(mContainer));
    NS_ASSERTION(docShellTreeNode, "mContainer has to be a nsIDocShellTreeNode");
    SetIsPrintingInDocShellTree(docShellTreeNode, aIsPrintPreview, PR_TRUE);
  }
#endif
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

static void ResetFocusState(nsIDocShell* aDocShell);

void
DocumentViewerImpl::ReturnToGalleyPresentation()
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (!GetIsPrintPreview()) {
    NS_ERROR("Wow, we should never get here!");
    return;
  }

  SetIsPrintPreview(PR_FALSE);

  mPrintEngine->TurnScriptingOn(PR_TRUE);
  mPrintEngine->Destroy();
  mPrintEngine = nsnull;

  mViewManager->EnableRefresh(NS_VMREFRESH_DEFERRED);

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  ResetFocusState(docShell);

  Show();

#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

//------------------------------------------------------------
// Reset ESM focus for all descendent doc shells.
static void
ResetFocusState(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  aDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                   nsIDocShell::ENUMERATE_FORWARDS,
                                   getter_AddRefs(docShellEnumerator));
  
  nsCOMPtr<nsIDocShell> currentDocShell;
  nsCOMPtr<nsISupports> currentContainer;
  PRBool hasMoreDocShells;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
         && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    currentDocShell = do_QueryInterface(currentContainer);
    if (!currentDocShell) {
      break;
    }
    nsCOMPtr<nsPresContext> presContext;
    currentDocShell->GetPresContext(getter_AddRefs(presContext));
    nsIEventStateManager* esm =
      presContext ? presContext->EventStateManager() : nsnull;
    if (esm) {
       esm->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
       esm->SetFocusedContent(nsnull);
    }
  }
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
      mPrintEngine = nsnull;
    }

    // We are done printing, now cleanup 
    if (mDeferredWindowClose) {
      mDeferredWindowClose = PR_FALSE;
      nsCOMPtr<nsISupports> container = do_QueryReferent(mContainer);
      nsCOMPtr<nsIDOMWindowInternal> win = do_GetInterface(container);
      if (win)
        win->Close();
    } else if (mClosingWhilePrinting) {
      if (mDocument) {
        mDocument->SetScriptGlobalObject(nsnull);
        mDocument->Destroy();
        mDocument = nsnull;
      }
      mClosingWhilePrinting = PR_FALSE;
      NS_RELEASE_THIS();
    }
  }
#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

NS_IMETHODIMP DocumentViewerImpl::SetPageMode(PRBool aPageMode, nsIPrintSettings* aPrintSettings)
{
  mIsPageMode = aPageMode;
  // Get the current size of what is being viewed
  nsRect bounds;
  mWindow->GetBounds(bounds);

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

  if (mPresContext) {
    mPresContext->SetContainer(nsnull);
    mPresContext->SetLinkHandler(nsnull);
  }

  mPresShell    = nsnull;
  mPresContext  = nsnull;
  mViewManager  = nsnull;
  mWindow       = nsnull;

  NS_ENSURE_STATE(mDocument);
  if (aPageMode)
  {    
    mPresContext =
      new nsPresContext(mDocument, nsPresContext::eContext_PageLayout);
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);
    mPresContext->SetPaginatedScrolling(PR_TRUE);
    mPresContext->SetPrintSettings(aPrintSettings);
    nsresult rv = mPresContext->Init(mDeviceContext);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  InitInternal(mParentWidget, nsnull, mDeviceContext, bounds, PR_TRUE, PR_FALSE, PR_FALSE);
  mViewManager->EnableRefresh(NS_VMREFRESH_NO_SYNC);

  Show();
  return NS_OK;
}

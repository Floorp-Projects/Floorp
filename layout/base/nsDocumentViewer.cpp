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
#include "mozilla/FunctionTimer.h"
#include "nsIDocumentViewerPrint.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsCSSStyleSheet.h"
#include "nsIFrame.h"
#include "nsSubDocumentFrame.h"

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
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIViewManager.h"
#include "nsIView.h"

#include "nsIPageSequenceFrame.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "mozilla/css/Loader.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
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
#include "nsXULPopupManager.h"
#endif
#include "nsPrintfCString.h"

#include "nsIClipboardHelper.h"

#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsJSEnvironment.h"
#include "nsFocusManager.h"

#include "nsIScrollableFrame.h"
#include "nsIHTMLDocument.h"
#include "nsITimelineService.h"
#include "nsGfxCIID.h"
#include "nsStyleSheetService.h"
#include "nsURILoader.h"

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
#include "nsIDOMEventTarget.h"
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
  DocumentViewerImpl();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIDocumentViewer interface...
  NS_IMETHOD GetPresShell(nsIPresShell** aResult);
  NS_IMETHOD GetPresContext(nsPresContext** aResult);
  NS_IMETHOD SetDocumentInternal(nsIDocument* aDocument,
                                 PRBool aForceReuseInnerWindow);
  /**
   * Find the view to use as the container view for MakeWindow. Returns
   * null if this will be the root of a view manager hierarchy. In that
   * case, if mParentWidget is null then this document should not even
   * be displayed.
   */
  virtual nsIView* FindContainerView();

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
  /**
   * Creates a view manager, root view, and widget for the root view, setting
   * mViewManager and mWindow.
   * @param aSize the initial size in appunits
   * @param aContainerView the container view to hook our root view up
   * to as a child, or null if this will be the root view manager
   */
  nsresult MakeWindow(const nsSize& aSize, nsIView* aContainerView);

  /**
   * Create our device context
   */
  nsresult CreateDeviceContext(nsIView* aContainerView);

  /**
   * If aDoCreation is true, this creates the device context, creates a
   * prescontext if necessary, and calls MakeWindow.
   */
  nsresult InitInternal(nsIWidget* aParentWidget,
                        nsISupports *aState,
                        const nsIntRect& aBounds,
                        PRBool aDoCreation,
                        PRBool aNeedMakeCX = PR_TRUE);
  /**
   * @param aDoInitialReflow set to true if you want to kick off the initial
   * reflow
   */
  nsresult InitPresentationStuff(PRBool aDoInitialReflow);

  nsresult GetPopupNode(nsIDOMNode** aNode);
  nsresult GetPopupLinkNode(nsIDOMNode** aNode);
  nsresult GetPopupImageNode(nsIImageLoadingContent** aNode);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

  nsresult GetDocumentSelection(nsISelection **aSelection);

  void DestroyPresShell();

#ifdef NS_PRINTING
  // Called when the DocViewer is notified that the state
  // of Printing or PP has changed
  void SetIsPrintingInDocShellTree(nsIDocShellTreeNode* aParentNode, 
                                   PRBool               aIsPrintingOrPP, 
                                   PRBool               aStartAtTop);
#endif // NS_PRINTING

protected:
  // These return the current shell/prescontext etc.
  nsIPresShell* GetPresShell();
  nsPresContext* GetPresContext();
  nsIViewManager* GetViewManager();

  void DetachFromTopLevelWidget();

  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  nsWeakPtr mContainer; // it owns me!
  nsCOMPtr<nsIDeviceContext> mDeviceContext;  // We create and own this baby

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // may be null
  nsCOMPtr<nsIViewManager> mViewManager;
  nsRefPtr<nsPresContext>  mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  nsCOMPtr<nsIDOMFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;
  nsCOMPtr<nsISHEntry> mSHEntry;

  nsIWidget* mParentWidget; // purposely won't be ref counted.  May be null
  PRBool mAttachedToParent; // view is attached to the parent widget

  nsIntRect mBounds;

  // mTextZoom/mPageZoom record the textzoom/pagezoom of the first (galley)
  // presshell only.
  float mTextZoom;      // Text zoom, defaults to 1.0
  float mPageZoom;

  PRInt16 mNumURLStarts;
  PRInt16 mDestroyRefCount;    // a second "refcount" for the document viewer's "destroy"

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
  unsigned                         mPrintPreviewZoomed : 1;

  // These data members support delayed printing when the document is loading
  unsigned                         mPrintIsPending : 1;
  unsigned                         mPrintDocIsFullyLoaded : 1;
  nsCOMPtr<nsIPrintSettings>       mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;

  nsCOMPtr<nsPrintEngine>          mPrintEngine;
  float                            mOriginalPrintPreviewScale;
  float                            mPrintPreviewZoom;
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
  PRPackedBool mCallerIsClosingWindow;
  PRPackedBool mInitializedForPrintPreview;
  PRPackedBool mHidden;
};

//------------------------------------------------------------------
// DocumentViewerImpl
//------------------------------------------------------------------
// Class IDs
static NS_DEFINE_CID(kViewManagerCID,       NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kDeviceContextCID,     NS_DEVICE_CONTEXT_CID);

//------------------------------------------------------------------
nsresult
NS_NewDocumentViewer(nsIDocumentViewer** aResult)
{
  *aResult = new DocumentViewerImpl();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

void DocumentViewerImpl::PrepareToStartLoad()
{
  mStopped          = PR_FALSE;
  mLoaded           = PR_FALSE;
  mAttachedToParent = PR_FALSE;
  mDeferredWindowClose = PR_FALSE;
  mCallerIsClosingWindow = PR_FALSE;

#ifdef NS_PRINTING
  mPrintIsPending        = PR_FALSE;
  mPrintDocIsFullyLoaded = PR_FALSE;
  mClosingWhilePrinting  = PR_FALSE;

  // Make sure we have destroyed it and cleared the data member
  if (mPrintEngine) {
    mPrintEngine->Destroy();
    mPrintEngine = nsnull;
#ifdef NS_PRINT_PREVIEW
    SetIsPrintPreview(PR_FALSE);
#endif
  }

#ifdef NS_DEBUG
  mDebugFile = nsnull;
#endif

#endif // NS_PRINTING
}

// Note: operator new zeros our memory, so no need to init things to null.
DocumentViewerImpl::DocumentViewerImpl()
  : mTextZoom(1.0), mPageZoom(1.0),
    mIsSticky(PR_TRUE),
#ifdef NS_PRINT_PREVIEW
    mPrintPreviewZoom(1.0),
#endif
    mHintCharsetSource(kCharsetUninitialized),
    mInitializedForPrintPreview(PR_FALSE),
    mHidden(PR_FALSE)
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
        if (mDocument && parent_doc->GetSubDocumentFor(content) != mDocument) {
          mDocument->SuppressEventHandling(parent_doc->EventHandlingSuppressed());
        }
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
                         const nsIntRect& aBounds)
{
  return InitInternal(aParentWidget, nsnull, aBounds, PR_TRUE);
}

nsresult
DocumentViewerImpl::InitPresentationStuff(PRBool aDoInitialReflow)
{
  if (GetIsPrintPreview())
    return NS_OK;

  NS_ASSERTION(!mPresShell,
               "Someone should have destroyed the presshell!");

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
  nscoord width = mPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel() * mBounds.width;
  nscoord height = mPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel() * mBounds.height;

  mViewManager->SetWindowDimensions(width, height);
  mPresContext->SetTextZoom(mTextZoom);
  mPresContext->SetFullZoom(mPageZoom);

  if (aDoInitialReflow) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLFrameSetElement> frameset =
        do_QueryInterface(mDocument->GetRootElement());
      htmlDoc->SetIsFrameset(frameset != nsnull);
    }

    nsCOMPtr<nsIPresShell> shellGrip = mPresShell;
    // Initial reflow
    mPresShell->InitialReflow(width, height);
  } else {
    // Store the visible area so it's available for other callers of
    // InitialReflow, like nsContentSink::StartLayout.
    mPresContext->SetVisibleArea(nsRect(0, 0, width, height));
  }

  // now register ourselves as a selection listener, so that we get
  // called when the selection changes in the window
  if (!mSelectionListener) {
    nsDocViewerSelectionListener *selectionListener =
      new nsDocViewerSelectionListener();
    NS_ENSURE_TRUE(selectionListener, NS_ERROR_OUT_OF_MEMORY);

    selectionListener->Init(this);

    // mSelectionListener is a owning reference
    mSelectionListener = selectionListener;
  }

  nsCOMPtr<nsISelection> selection;
  rv = GetDocumentSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
  rv = selPrivate->AddSelectionListener(mSelectionListener);
  if (NS_FAILED(rv))
    return rv;

  // Save old listener so we can unregister it
  nsCOMPtr<nsIDOMFocusListener> oldFocusListener = mFocusListener;

  // focus listener
  //
  // now register ourselves as a focus listener, so that we get called
  // when the focus changes in the window
  nsDocViewerFocusListener *focusListener = new nsDocViewerFocusListener();
  NS_ENSURE_TRUE(focusListener, NS_ERROR_OUT_OF_MEMORY);

  focusListener->Init(this);

  // mFocusListener is a strong reference
  mFocusListener = focusListener;

  if (mDocument) {
    rv = mDocument->AddEventListenerByIID(mFocusListener,
                                          NS_GET_IID(nsIDOMFocusListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register focus listener");
    if (oldFocusListener) {
      rv = mDocument->RemoveEventListenerByIID(oldFocusListener,
                                               NS_GET_IID(nsIDOMFocusListener));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove focus listener");
    }
  }

  if (aDoInitialReflow && mDocument) {
    mDocument->ScrollToRef();
  }

  return NS_OK;
}

static nsPresContext*
CreatePresContext(nsIDocument* aDocument,
                  nsPresContext::nsPresContextType aType,
                  nsIView* aContainerView)
{
  if (aContainerView)
    return new nsPresContext(aDocument, aType);
  return new nsRootPresContext(aDocument, aType);
}

//-----------------------------------------------
// This method can be used to initial the "presentation"
// The aDoCreation indicates whether it should create
// all the new objects or just initialize the existing ones
nsresult
DocumentViewerImpl::InitInternal(nsIWidget* aParentWidget,
                                 nsISupports *aState,
                                 const nsIntRect& aBounds,
                                 PRBool aDoCreation,
                                 PRBool aNeedMakeCX /*= PR_TRUE*/)
{
  // We don't want any scripts to run here. That can cause flushing,
  // which can cause reentry into initialization of this document viewer,
  // which would be disastrous.
  nsAutoScriptBlocker blockScripts;

  mParentWidget = aParentWidget; // not ref counted
  mBounds = aBounds;

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  nsIView* containerView = FindContainerView();

  PRBool makeCX = PR_FALSE;
  if (aDoCreation) {
    nsresult rv = CreateDeviceContext(containerView);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXXbz this is a nasty hack to do with the fact that we create
    // presentations both in Init() and in Show()...  Ideally we would only do
    // it in one place (Show()) and require that callers call init(), open(),
    // show() in that order or something.
    if (!mPresContext &&
        (aParentWidget || containerView || mDocument->IsBeingUsedAsImage() ||
         (mDocument->GetDisplayDocument() &&
          mDocument->GetDisplayDocument()->GetShell()))) {
      // Create presentation context
      if (mIsPageMode) {
        //Presentation context already created in SetPageMode which is calling this method
      } else {
        mPresContext = CreatePresContext(mDocument,
            nsPresContext::eContext_Galley, containerView);
      }
      NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

      nsresult rv = mPresContext->Init(mDeviceContext); 
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

      rv = MakeWindow(nsSize(mPresContext->DevPixelsToAppUnits(aBounds.width),
                             mPresContext->DevPixelsToAppUnits(aBounds.height)),
                      containerView);
      NS_ENSURE_SUCCESS(rv, rv);
      Hide();

#ifdef NS_PRINT_PREVIEW
      if (mIsPageMode) {
        // I'm leaving this in a broken state for the moment; we should
        // be measuring/scaling with the print device context, not the
        // screen device context, but this is good enough to allow
        // printing reftests to work.
#if 0
        nsCOMPtr<nsIDeviceContextSpec> devspec =
          do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        // mWindow has been initialized by preceding call to MakeWindow
        rv = devspec->Init(mWindow, mPresContext->GetPrintSettings(), PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIDeviceContext> devctx =
          do_CreateInstance("@mozilla.org/gfx/devicecontext;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = devctx->InitForPrinting(devspec);
        NS_ENSURE_SUCCESS(rv, rv);
        // XXX I'm breaking this code; I'm not sure I really want to mess with
        // the document viewer at the moment to get the right device context
        // (this won't break anyone, since page layout mode was never really
        // usable)
#endif
        double pageWidth = 0, pageHeight = 0;
        mPresContext->GetPrintSettings()->GetEffectivePageSize(&pageWidth,
                                                               &pageHeight);
        mPresContext->SetPageSize(
          nsSize(mPresContext->CSSTwipsToAppUnits(NSToIntFloor(pageWidth)),
                 mPresContext->CSSTwipsToAppUnits(NSToIntFloor(pageHeight))));
        mPresContext->SetIsRootPaginatedDocument(PR_TRUE);
        mPresContext->SetPageScale(1.0f);
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

    // Set script-context-owner in the document

    nsCOMPtr<nsPIDOMWindow> window;
    requestor->GetInterface(NS_GET_IID(nsPIDOMWindow),
                            getter_AddRefs(window));

    if (window) {
      nsCOMPtr<nsIDocument> curDoc =
        do_QueryInterface(window->GetExtantDocument());
      if (!mIsPageMode || curDoc != mDocument) {
        window->SetNewDocument(mDocument, aState, PR_FALSE);
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
  NS_TIME_FUNCTION;
  /* We need to protect ourself against auto-destruction in case the
     window is closed while processing the OnLoad event.  See bug
     http://bugzilla.mozilla.org/show_bug.cgi?id=78445 for more
     explanation.
  */
  nsCOMPtr<nsIDocumentViewer> kungFuDeathGrip(this);

  // Flush out layout so it's up-to-date by the time onload is called.
  // Note that this could destroy the window, so do this before
  // checking for our mDocument and its window.
  if (mPresShell && !mStopped) {
    // Hold strong ref because this could conceivably run script
    nsCOMPtr<nsIPresShell> shell = mPresShell;
    shell->FlushPendingNotifications(Flush_Layout);
  }

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  // First, get the window from the document...
  nsPIDOMWindow *window = mDocument->GetWindow();

  mLoaded = PR_TRUE;

  // Now, fire either an OnLoad or OnError event to the document...
  PRBool restoring = PR_FALSE;
  // XXXbz imagelib kills off the document load for a full-page image with
  // NS_ERROR_PARSED_DATA_CACHED if it's in the cache.  So we want to treat
  // that one as a success code; otherwise whether we fire onload for the image
  // will depend on whether it's cached!
  if(window &&
     (NS_SUCCEEDED(aStatus) || aStatus == NS_ERROR_PARSED_DATA_CACHED)) {
    if (mDocument)
      mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);
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
  if (mDocument) {
    // Re-get window, since it might have changed during above firing of onload
    window = mDocument->GetWindow();
    if (window) {
      nsIDocShell *docShell = window->GetDocShell();
      PRBool isInUnload;
      if (docShell && NS_SUCCEEDED(docShell->GetIsInUnload(&isInUnload)) &&
          !isInUnload) {
        mDocument->OnPageShow(restoring, nsnull);
      }
    }
  }

  // Now that the document has loaded, we can tell the presshell
  // to unsuppress painting.
  if (mPresShell && !mStopped) {
    nsCOMPtr<nsIPresShell> shellDeathGrip(mPresShell);
    mPresShell->UnsuppressPainting();
    // mPresShell could have been removed now, see bug 378682/421432
    if (mPresShell) {
      mPresShell->ScrollToAnchor();
    }
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

  if (!mStopped && window) {
    window->DispatchSyncPopState();
  }

  return rv;
}

NS_IMETHODIMP
DocumentViewerImpl::PermitUnload(PRBool aCallerClosesWindow, PRBool *aPermitUnload)
{
  *aPermitUnload = PR_TRUE;

  if (!mDocument || mInPermitUnload || mCallerIsClosingWindow) {
    return NS_OK;
  }

  // First, get the script global object from the document...
  nsPIDOMWindow *window = mDocument->GetWindow();

  if (!window) {
    // This is odd, but not fatal
    NS_WARNING("window not set for document!");
    return NS_OK;
  }

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "This is unsafe");

  // Now, fire an BeforeUnload event to the document and see if it's ok
  // to unload...
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("beforeunloadevent"),
                        getter_AddRefs(event));
  nsCOMPtr<nsIDOMBeforeUnloadEvent> beforeUnload = do_QueryInterface(event);
  nsCOMPtr<nsIPrivateDOMEvent> pEvent = do_QueryInterface(beforeUnload);
  NS_ENSURE_STATE(pEvent);
  nsresult rv = event->InitEvent(NS_LITERAL_STRING("beforeunload"),
                                 PR_FALSE, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX Dispatching to |window|, but using |document| as the target.
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mDocument);
  pEvent->SetTarget(target);
  pEvent->SetTrusted(PR_TRUE);

  // In evil cases we might be destroyed while handling the
  // onbeforeunload event, don't let that happen. (see also bug#331040)
  nsRefPtr<DocumentViewerImpl> kungFuDeathGrip(this);

  {
    // Never permit popups from the beforeunload handler, no matter
    // how we get here.
    nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

    mInPermitUnload = PR_TRUE;
    nsEventDispatcher::DispatchDOMEvent(window, nsnull, event, mPresContext,
                                        nsnull);
    mInPermitUnload = PR_FALSE;
  }

  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryReferent(mContainer));
  nsAutoString text;
  beforeUnload->GetReturnValue(text);
  if (pEvent->GetInternalNSEvent()->flags & NS_EVENT_FLAG_NO_DEFAULT ||
      !text.IsEmpty()) {
    // Ask the user if it's ok to unload the current page

    nsCOMPtr<nsIPrompt> prompt = do_GetInterface(docShellNode);

    if (prompt) {
      nsXPIDLString title, message, stayLabel, leaveLabel;
      rv  = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadTitle",
                                               title);
      rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadMessage",
                                               message);
      rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadLeaveButton",
                                               leaveLabel);
      rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadStayButton",
                                               stayLabel);

      if (NS_FAILED(rv) || !title || !message || !stayLabel || !leaveLabel) {
        NS_ERROR("Failed to get strings from dom.properties!");
        return NS_OK;
      }

      PRBool dummy;
      PRInt32 buttonPressed = 0;
      PRUint32 buttonFlags = (nsIPrompt::BUTTON_POS_0_DEFAULT |
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) |
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_1));

      rv = prompt->ConfirmEx(title, message, buttonFlags,
                             leaveLabel, stayLabel, nsnull, nsnull,
                             &dummy, &buttonPressed);
      NS_ENSURE_SUCCESS(rv, rv);

      // Button 0 == leave, button 1 == stay
      *aPermitUnload = (buttonPressed == 0);
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
          cv->PermitUnload(aCallerClosesWindow, aPermitUnload);
        }
      }
    }
  }

  if (aCallerClosesWindow && *aPermitUnload)
    mCallerIsClosingWindow = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::ResetCloseWindow()
{
  mCallerIsClosingWindow = PR_FALSE;

  nsCOMPtr<nsIDocShellTreeNode> docShellNode(do_QueryReferent(mContainer));
  if (docShellNode) {
    PRInt32 childCount;
    docShellNode->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      docShellNode->GetChildAt(i, getter_AddRefs(item));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(item));

      if (docShell) {
        nsCOMPtr<nsIContentViewer> cv;
        docShell->GetContentViewer(getter_AddRefs(cv));

        if (cv) {
          cv->ResetCloseWindow();
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::PageHide(PRBool aIsUnload)
{
  mHidden = PR_TRUE;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument->OnPageHide(!aIsUnload, nsnull);

  // inform the window so that the focus state is reset.
  NS_ENSURE_STATE(mDocument);
  nsPIDOMWindow *window = mDocument->GetWindow();
  if (window)
    window->PageHidden();

  if (aIsUnload) {
    // if Destroy() was called during OnPageHide(), mDocument is nsnull.
    NS_ENSURE_STATE(mDocument);

    // First, get the window from the document...
    nsPIDOMWindow *window = mDocument->GetWindow();

    if (!window) {
      // Fail if no window is available...
      NS_WARNING("window not set for document!");
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

#ifdef MOZ_XUL
  // look for open menupopups and close them after the unload event, in case
  // the unload event listeners open any new popups
  nsContentUtils::HidePopupsInDocument(mDocument);
#endif

  return NS_OK;
}

static void
AttachContainerRecurse(nsIDocShell* aShell)
{
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(viewer);
  if (docViewer) {
    nsIDocument* doc = docViewer->GetDocument();
    if (doc) {
      doc->SetContainer(aShell);
    }
    nsRefPtr<nsPresContext> pc;
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

  if (mDocument)
    mDocument->SetContainer(nsCOMPtr<nsISupports>(do_QueryReferent(mContainer)));

  nsresult rv = InitInternal(mParentWidget, aState, mBounds, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mPresShell)
    mPresShell->SetForwardingContainer(nsnull);

  // Rehook the child presentations.  The child shells are still in
  // session history, so get them from there.

  if (aSHEntry) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    PRInt32 itemIndex = 0;
    while (NS_SUCCEEDED(aSHEntry->ChildShellAt(itemIndex++,
                                               getter_AddRefs(item))) && item) {
      AttachContainerRecurse(nsCOMPtr<nsIDocShell>(do_QueryInterface(item)));
    }
  }
  
  SyncParentSubDocMap();

  if (mFocusListener && mDocument) {
    mDocument->AddEventListenerByIID(mFocusListener,
                                     NS_GET_IID(nsIDOMFocusListener));
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
  if (mPrintEngine && !mClosingWhilePrinting) {
    mClosingWhilePrinting = PR_TRUE;
  } else
#endif
    {
      // out of band cleanup of docshell
      mDocument->SetScriptGlobalObject(nsnull);

      if (!mSHEntry && mDocument)
        mDocument->RemovedFromDocShell();
    }

  if (mFocusListener && mDocument) {
    mDocument->RemoveEventListenerByIID(mFocusListener,
                                        NS_GET_IID(nsIDOMFocusListener));
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
    nsIDocument* doc = docViewer->GetDocument();
    if (doc) {
      doc->SetContainer(nsnull);
    }
    nsRefPtr<nsPresContext> pc;
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
  // Here is where we check to see if the document was still being prepared 
  // for printing when it was asked to be destroy from someone externally
  // This usually happens if the document is unloaded while the user is in the
  // Print Dialog
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
#ifdef NS_PRINT_PREVIEW
    PRBool doingPrintPreview;
    mPrintEngine->GetDoingPrintPreview(&doingPrintPreview);
    if (doingPrintPreview) {
      mPrintEngine->FinishPrintPreview();
    }
#endif

    mPrintEngine->Destroy();
    mPrintEngine = nsnull;
  }
#endif

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nsnull;
  }

  mDeviceContext = nsnull;

  if (mPresShell) {
    DestroyPresShell();
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

  if (!mHidden && (mLoaded || mStopped) && mPresContext && !mSHEntry)
    mPresContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);

  mStopped = PR_TRUE;

  if (!mLoaded && mPresShell) {
    // Well, we might as well paint what we have so far.
    nsCOMPtr<nsIPresShell> shellDeathGrip(mPresShell); // bug 378682
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

NS_IMETHODIMP_(nsIDocument *)
DocumentViewerImpl::GetDocument()
{
  return mDocument;
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

  if (!aDocument)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDocument> newDoc = do_QueryInterface(aDocument);
  NS_ENSURE_TRUE(newDoc, NS_ERROR_UNEXPECTED);

  return SetDocumentInternal(newDoc, PR_FALSE);
}

NS_IMETHODIMP
DocumentViewerImpl::SetDocumentInternal(nsIDocument* aDocument,
                                        PRBool aForceReuseInnerWindow)
{

  // Set new container
  nsCOMPtr<nsISupports> container = do_QueryReferent(mContainer);
  aDocument->SetContainer(container);

  if (mDocument != aDocument) {
    // Replace the old document with the new one. Do this only when
    // the new document really is a new document.
    mDocument = aDocument;

    // Set the script global object on the new document
    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(container);
    if (window) {
      window->SetNewDocument(aDocument, nsnull, aForceReuseInnerWindow);
    }

    // Clear the list of old child docshells. CChild docshells for the new
    // document will be constructed as frames are created.
    if (!aDocument->IsStaticDocument()) {
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
  }

  nsresult rv = SyncParentSubDocMap();
  NS_ENSURE_SUCCESS(rv, rv);

  // Replace the current pres shell with a new shell for the new document

  nsCOMPtr<nsILinkHandler> linkHandler;
  if (mPresShell) {
    nsSize currentSize(0, 0);

    if (mViewManager) {
      mViewManager->GetWindowDimensions(&currentSize.width, &currentSize.height);
    }

    if (mPresContext) {
      // Save the linkhandler (nsPresShell::Destroy removes it from
      // mPresContext).
      linkHandler = mPresContext->GetLinkHandler();
    }

    DestroyPresShell();

    nsIView* containerView = FindContainerView();

    // This destroys the root view because it was associated with the root frame,
    // which has been torn down. Recreate the viewmanager and root view.
    MakeWindow(currentSize, containerView);
  }

  // And if we're already given a prescontext...
  if (mPresContext) {
    // If we had a linkHandler and it got removed, put it back.
    if (linkHandler) {
      mPresContext->SetLinkHandler(linkHandler);
    }

    rv = InitPresentationStuff(PR_FALSE);
  }

  return rv;
}

nsIPresShell*
DocumentViewerImpl::GetPresShell()
{
  return mPresShell;
}

nsPresContext*
DocumentViewerImpl::GetPresContext()
{
  return mPresContext;
}

nsIViewManager*
DocumentViewerImpl::GetViewManager()
{
  return mViewManager;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresShell(nsIPresShell** aResult)
{
  nsIPresShell* shell = GetPresShell();
  NS_IF_ADDREF(*aResult = shell);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetPresContext(nsPresContext** aResult)
{
  nsPresContext* pc = GetPresContext();
  NS_IF_ADDREF(*aResult = pc);
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetBounds(nsIntRect& aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  aResult = mBounds;
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
    // It's very important that if this ever gets changed the code
    // before the RestorePresentation call in nsDocShell::InternalLoad
    // be changed accordingly.
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
DocumentViewerImpl::SetBounds(const nsIntRect& aBounds)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  mBounds = aBounds;
  if (mWindow) {
    // When attached to a top level window, change the client area, not the
    // window frame.
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow.
    if (mAttachedToParent)
      mWindow->ResizeClient(aBounds.x, aBounds.y,
                            aBounds.width, aBounds.height,
                            PR_FALSE);
    else
      mWindow->Resize(aBounds.x, aBounds.y,
                      aBounds.width, aBounds.height,
                      PR_FALSE);
  } else if (mPresContext && mViewManager) {
    PRInt32 p2a = mPresContext->AppUnitsPerDevPixel();
    mViewManager->SetWindowDimensions(NSIntPixelsToAppUnits(mBounds.width, p2a),
                                      NSIntPixelsToAppUnits(mBounds.height, p2a));
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

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Move(PRInt32 aX, PRInt32 aY)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  mBounds.MoveTo(aX, aY);
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
    // When attached to a top level xul window, we do not need to call
    // Show on the widget. Underlying window management code handles
    // this when the window is initialized.
    if (!mAttachedToParent) {
      mWindow->Show(PR_TRUE);
    }
  }

  if (mDocument && !mPresShell) {
    NS_ASSERTION(!mWindow, "Window already created but no presshell?");

    nsCOMPtr<nsIBaseWindow> base_win(do_QueryReferent(mContainer));
    if (base_win) {
      base_win->GetParentWidget(&mParentWidget);
      if (mParentWidget) {
        mParentWidget->Release(); // GetParentWidget AddRefs, but mParentWidget is weak
      }
    }

    nsIView* containerView = FindContainerView();

    nsresult rv = CreateDeviceContext(containerView);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create presentation context
    NS_ASSERTION(!mPresContext, "Shouldn't have a prescontext if we have no shell!");
    mPresContext = CreatePresContext(mDocument,
        nsPresContext::eContext_Galley, containerView);
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

    rv = mPresContext->Init(mDeviceContext);
    if (NS_FAILED(rv)) {
      mPresContext = nsnull;
      return rv;
    }

    rv = MakeWindow(nsSize(mPresContext->DevPixelsToAppUnits(mBounds.width),
                           mPresContext->DevPixelsToAppUnits(mBounds.height)),
                           containerView);
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

      rv = InitPresentationStuff(mDocument->MayStartLayout());
    }

    // If we get here the document load has already started and the
    // window is shown because some JS on the page caused it to be
    // shown...

    if (mPresShell) {
      nsCOMPtr<nsIPresShell> shellDeathGrip(mPresShell); // bug 378682
      mPresShell->UnsuppressPainting();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::Hide(void)
{
  if (!mAttachedToParent && mWindow) {
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

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    nsCOMPtr<nsILayoutHistoryState> layoutState;
    mPresShell->CaptureHistoryState(getter_AddRefs(layoutState), PR_TRUE);
  }

  DestroyPresShell();

  // Clear weak refs
  mPresContext->SetContainer(nsnull);
  mPresContext->SetLinkHandler(nsnull);                             

  mPresContext   = nsnull;
  mViewManager   = nsnull;
  mWindow        = nsnull;
  mDeviceContext = nsnull;
  mParentWidget  = nsnull;

  nsCOMPtr<nsIBaseWindow> base_win(do_QueryReferent(mContainer));

  if (base_win && !mAttachedToParent) {
    base_win->SetParentWidget(nsnull);
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

static PRBool
AppendAgentSheet(nsIStyleSheet *aSheet, void *aData)
{
  nsStyleSet *styleSet = static_cast<nsStyleSet*>(aData);
  styleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, aSheet);
  return PR_TRUE;
}

static PRBool
PrependUserSheet(nsIStyleSheet *aSheet, void *aData)
{
  nsStyleSet *styleSet = static_cast<nsStyleSet*>(aData);
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
  nsStyleSet *styleSet = new nsStyleSet();
  if (!styleSet) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  styleSet->BeginUpdate();
  
  // The document will fill in the document sheets when we create the presshell
  
  // Handle the user sheets.
#ifdef DEBUG
  nsCOMPtr<nsISupports> debugDocContainer = aDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> debugDocShell(do_QueryReferent(mContainer));
  NS_ASSERTION(SameCOMIdentity(debugDocContainer, debugDocShell),
               "Unexpected containers");
#endif
  nsCSSStyleSheet* sheet = nsnull;
  if (nsContentUtils::IsInChromeDocshell(aDocument)) {
    sheet = nsLayoutStylesheetCache::UserChromeSheet();
  }
  else {
    sheet = nsLayoutStylesheetCache::UserContentSheet();
  }

  if (sheet)
    styleSet->AppendStyleSheet(nsStyleSet::eUserSheet, sheet);

  // Append chrome sheets (scrollbars + forms).
  PRBool shouldOverride = PR_FALSE;
  // We don't want a docshell here for external resource docs, so just
  // look at mContainer.
  nsCOMPtr<nsIDocShell> ds(do_QueryReferent(mContainer));
  nsCOMPtr<nsIDOMEventTarget> chromeHandler;
  nsCOMPtr<nsIURI> uri;
  nsRefPtr<nsCSSStyleSheet> csssheet;

  if (ds) {
    ds->GetChromeEventHandler(getter_AddRefs(chromeHandler));
  }
  if (chromeHandler) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(chromeHandler));
    nsCOMPtr<nsIContent> content(do_QueryInterface(elt));
    if (elt && content) {
      nsCOMPtr<nsIURI> baseURI = content->GetBaseURI();

      nsAutoString sheets;
      elt->GetAttribute(NS_LITERAL_STRING("usechromesheets"), sheets);
      if (!sheets.IsEmpty() && baseURI) {
        nsRefPtr<mozilla::css::Loader> cssLoader = new mozilla::css::Loader();

        char *str = ToNewCString(sheets);
        char *newStr = str;
        char *token;
        while ( (token = nsCRT::strtok(newStr, ", ", &newStr)) ) {
          NS_NewURI(getter_AddRefs(uri), nsDependentCString(token), nsnull,
                    baseURI);
          if (!uri) continue;

          cssLoader->LoadSheetSync(uri, getter_AddRefs(csssheet));
          if (!csssheet) continue;

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

  // Make sure to clone the quirk sheet so that it can be usefully
  // enabled/disabled as needed.
  nsRefPtr<nsCSSStyleSheet> quirkClone;
  nsCSSStyleSheet* quirkSheet;
  if (!nsLayoutStylesheetCache::UASheet() ||
      !(quirkSheet = nsLayoutStylesheetCache::QuirkSheet()) ||
      !(quirkClone = quirkSheet->Clone(nsnull, nsnull, nsnull, nsnull)) ||
      !sheet) {
    delete styleSet;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // quirk.css needs to come after the regular UA sheet (or more precisely,
  // after the html.css and so forth that the UA sheet imports).
  styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet, quirkClone);
  styleSet->SetQuirkStyleSheet(quirkClone);
  styleSet->PrependStyleSheet(nsStyleSet::eAgentSheet,
                              nsLayoutStylesheetCache::UASheet());

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
DocumentViewerImpl::MakeWindow(const nsSize& aSize, nsIView* aContainerView)
{
  if (GetIsPrintPreview())
    return NS_OK;

  // Prior to creating a new widget, check to see if our parent is a base
  // chrome ui window. If so, drop the content into that widget instead of
  // creating a new child widget. This eliminates the main content child
  // widget we've had forever. Also allows for the recycling of the base
  // widget of each window, vs. creating/discarding child widgets for each
  // MakeWindow call. (Currently only implemented in windows widgets.)
#ifdef XP_WIN
  nsCOMPtr<nsIDocShellTreeItem> containerItem = do_QueryReferent(mContainer);
  if (mParentWidget && containerItem) {
    PRInt32 docType;
    nsWindowType winType;
    containerItem->GetItemType(&docType);
    mParentWidget->GetWindowType(winType);
    if ((winType == eWindowType_toplevel ||
         winType == eWindowType_dialog ||
         winType == eWindowType_invisible) &&
        docType == nsIDocShellTreeItem::typeChrome) {
      // If the old view is already attached to our parent, detach
      DetachFromTopLevelWidget();
      // Use the parent widget
      mAttachedToParent = PR_TRUE;
    }
  }
#endif

  nsresult rv;
  mViewManager = do_CreateInstance(kViewManagerCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsIDeviceContext *dx = mPresContext->DeviceContext();

  rv = mViewManager->Init(dx);
  if (NS_FAILED(rv))
    return rv;

  // The root view is always at 0,0.
  nsRect tbounds(nsPoint(0, 0), aSize);
  // Create a view
  nsIView* view = mViewManager->CreateView(tbounds, aContainerView);
  if (!view)
    return NS_ERROR_OUT_OF_MEMORY;

  // Create a widget if we were given a parent widget or don't have a
  // container view that we can hook up to without a widget.
  // Don't create widgets for ResourceDocs (external resources & svg images),
  // because when they're displayed, they're painted into *another* document's
  // widget.
  if (!mDocument->IsResourceDoc() &&
      (mParentWidget || !aContainerView)) {
    // pass in a native widget to be the parent widget ONLY if the view hierarchy will stand alone.
    // otherwise the view will find its own parent widget and "do the right thing" to
    // establish a parent/child widget relationship
    nsWidgetInitData initData;
    nsWidgetInitData* initDataPtr;
    if (!mParentWidget) {
      initDataPtr = &initData;
      initData.mWindowType = eWindowType_invisible;
    } else {
      initDataPtr = nsnull;
    }

    if (mAttachedToParent) {
      // Reuse the top level parent widget.
      rv = view->AttachToTopLevelWidget(mParentWidget);
    }
    else if (!aContainerView && mParentWidget) {
      rv = view->CreateWidgetForParent(mParentWidget, initDataPtr,
                                       PR_TRUE, PR_FALSE);
    }
    else {
      rv = view->CreateWidget(initDataPtr, PR_TRUE, PR_FALSE);
    }
    if (NS_FAILED(rv))
      return rv;
  }

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(view);

  mWindow = view->GetWidget();

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going to
  // the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

void
DocumentViewerImpl::DetachFromTopLevelWidget()
{
  if (mViewManager) {
    nsIView* oldView = nsnull;
    mViewManager->GetRootView(oldView);
    if (oldView && oldView->IsAttachedToTopLevel()) {
      oldView->DetachFromTopLevelWidget();
    }
  }
  mAttachedToParent = PR_FALSE;
}

nsIView*
DocumentViewerImpl::FindContainerView()
{
  nsIView* containerView = nsnull;

  if (mContainer) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem = do_QueryReferent(mContainer);
    nsCOMPtr<nsPIDOMWindow> pwin(do_GetInterface(docShellItem));
    if (pwin) {
      nsCOMPtr<nsIContent> containerElement = do_QueryInterface(pwin->GetFrameElementInternal());
      nsCOMPtr<nsIPresShell> parentPresShell;
      if (docShellItem) {
        nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem;
        docShellItem->GetParent(getter_AddRefs(parentDocShellItem));
        if (parentDocShellItem) {
          nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentDocShellItem);
          parentDocShell->GetPresShell(getter_AddRefs(parentPresShell));
        }
      }
      if (!parentPresShell && containerElement) {
        nsCOMPtr<nsIDocument> parentDoc = containerElement->GetCurrentDoc();
        if (parentDoc) {
          parentPresShell = parentDoc->GetShell();
        }
      }
      if (!containerElement) {
        NS_WARNING("Subdocument container has no content");
      } else if (!parentPresShell) {
        NS_WARNING("Subdocument container has no presshell");
      } else {
        nsIFrame* f = parentPresShell->GetRealPrimaryFrameFor(containerElement);
        if (f) {
          nsIFrame* subdocFrame = f->GetContentInsertionFrame();
          // subdocFrame might not be a subdocument frame; the frame
          // constructor can treat a <frame> as an inline in some XBL
          // cases. Treat that as display:none, the document is not
          // displayed.
          if (subdocFrame->GetType() == nsGkAtoms::subDocumentFrame) {
            NS_ASSERTION(subdocFrame->GetView(), "Subdoc frames must have views");
            nsIView* innerView =
              static_cast<nsSubDocumentFrame*>(subdocFrame)->EnsureInnerView();
            containerView = innerView;
          } else {
            NS_WARNING("Subdocument container has non-subdocument frame");
          }
        } else {
          NS_WARNING("Subdocument container has no frame");
        }
      }
    }
  }

  return containerView;
}

nsresult
DocumentViewerImpl::CreateDeviceContext(nsIView* aContainerView)
{
  NS_PRECONDITION(!mPresShell && !mWindow,
                  "This will screw up our existing presentation");
  NS_PRECONDITION(mDocument, "Gotta have a document here");
  
  nsIDocument* doc = mDocument->GetDisplayDocument();
  if (doc) {
    NS_ASSERTION(!aContainerView, "External resource document embedded somewhere?");
    // We want to use our display document's device context if possible
    nsIPresShell* shell = doc->GetShell();
    if (shell) {
      nsPresContext* ctx = shell->GetPresContext();
      if (ctx) {
        mDeviceContext = ctx->DeviceContext();
        return NS_OK;
      }
    }
  }
  
  // Create a device context even if we already have one, since our widget
  // might have changed.
  mDeviceContext = do_CreateInstance(kDeviceContextCID);
  NS_ENSURE_TRUE(mDeviceContext, NS_ERROR_FAILURE);
  nsIWidget* widget = nsnull;
  if (aContainerView) {
    widget = aContainerView->GetNearestWidget(nsnull);
  }
  // The device context needs a widget to be able to determine the screen it is on.
  if (!widget) {
    widget = mParentWidget;
  }
  if (widget) {
    widget = widget->GetTopLevelWidget();
  }

  mDeviceContext->Init(widget);
  return NS_OK;
}

// Return the selection for the document. Note that text fields have their
// own selection, which cannot be accessed with this method.
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

/* ========================================================================================
 * nsIContentViewerEdit
 * ======================================================================================== */

NS_IMETHODIMP DocumentViewerImpl::ClearSelection()
{
  nsresult rv;
  nsCOMPtr<nsISelection> selection;

  // use nsCopySupport::GetSelectionForCopy() ?
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

  // use nsCopySupport::GetSelectionForCopy() ?
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
    bodyNode = do_QueryInterface(mDocument->GetRootElement());
  }
  if (!bodyNode) return NS_ERROR_FAILURE;

  rv = selection->RemoveAllRanges();
  if (NS_FAILED(rv)) return rv;

  rv = selection->SelectAllChildren(bodyNode);
  return rv;
}

NS_IMETHODIMP DocumentViewerImpl::CopySelection()
{
  nsCopySupport::FireClipboardEvent(NS_COPY, mPresShell, nsnull);
  return NS_OK;
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
  NS_ENSURE_ARG_POINTER(aCopyable);
  *aCopyable = nsCopySupport::CanCopy(mDocument);
  return NS_OK;
}

/* AString getContents (in string mimeType, in boolean selectionOnly); */
NS_IMETHODIMP DocumentViewerImpl::GetContents(const char *mimeType, PRBool selectionOnly, nsAString& aOutValue)
{
  aOutValue.Truncate();

  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  // Now we have the selection.  Make sure it's nonzero:
  nsCOMPtr<nsISelection> sel;
  if (selectionOnly) {
    nsCopySupport::GetSelectionForCopy(mDocument, getter_AddRefs(sel));
    NS_ENSURE_TRUE(sel, NS_ERROR_FAILURE);
  
    PRBool isCollapsed;
    sel->GetIsCollapsed(&isCollapsed);
    if (isCollapsed)
      return NS_OK;
  }

  // call the copy code
  return nsCopySupport::GetContents(nsDependentCString(mimeType), 0, sel,
                                    mDocument, aOutValue);
}

/* readonly attribute boolean canGetContents; */
NS_IMETHODIMP DocumentViewerImpl::GetCanGetContents(PRBool *aCanGetContents)
{
  NS_ENSURE_ARG_POINTER(aCanGetContents);
  *aCanGetContents = PR_FALSE;
  NS_ENSURE_STATE(mDocument);
  *aCanGetContents = nsCopySupport::CanCopy(mDocument);
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

   // Tell the PresShell to scroll to the primary frame of the content.
   NS_ENSURE_SUCCESS(presShell->ScrollContentIntoView(content,
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

struct ZoomInfo
{
  float mZoom;
};

static void
SetChildTextZoom(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  struct ZoomInfo* ZoomInfo = (struct ZoomInfo*) aClosure;
  aChild->SetTextZoom(ZoomInfo->mZoom);
}

static void
SetChildFullZoom(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  struct ZoomInfo* ZoomInfo = (struct ZoomInfo*) aClosure;
  aChild->SetFullZoom(ZoomInfo->mZoom);
}

static PRBool
SetExtResourceTextZoom(nsIDocument* aDocument, void* aClosure)
{
  // Would it be better to enumerate external resource viewers instead?
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      struct ZoomInfo* ZoomInfo = static_cast<struct ZoomInfo*>(aClosure);
      ctxt->SetTextZoom(ZoomInfo->mZoom);
    }
  }

  return PR_TRUE;
}

static PRBool
SetExtResourceFullZoom(nsIDocument* aDocument, void* aClosure)
{
  // Would it be better to enumerate external resource viewers instead?
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      struct ZoomInfo* ZoomInfo = static_cast<struct ZoomInfo*>(aClosure);
      ctxt->SetFullZoom(ZoomInfo->mZoom);
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP
DocumentViewerImpl::SetTextZoom(float aTextZoom)
{
  if (GetIsPrintPreview()) {
    return NS_OK;
  }

  mTextZoom = aTextZoom;

  nsIViewManager::UpdateViewBatch batch(GetViewManager());
      
  // Set the text zoom on all children of mContainer (even if our zoom didn't
  // change, our children's zoom may be different, though it would be unusual).
  // Do this first, in case kids are auto-sizing and post reflow commands on
  // our presshell (which should be subsumed into our own style change reflow).
  struct ZoomInfo ZoomInfo = { aTextZoom };
  CallChildren(SetChildTextZoom, &ZoomInfo);

  // Now change our own zoom
  nsPresContext* pc = GetPresContext();
  if (pc && aTextZoom != mPresContext->TextZoom()) {
      pc->SetTextZoom(aTextZoom);
  }

  // And do the external resources
  mDocument->EnumerateExternalResources(SetExtResourceTextZoom, &ZoomInfo);

  batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
  
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);
  nsPresContext* pc = GetPresContext();
  *aTextZoom = pc ? pc->TextZoom() : 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::SetFullZoom(float aFullZoom)
{
#ifdef NS_PRINT_PREVIEW
  if (GetIsPrintPreview()) {
    nsPresContext* pc = GetPresContext();
    NS_ENSURE_TRUE(pc, NS_OK);
    nsCOMPtr<nsIPresShell> shell = pc->GetPresShell();
    NS_ENSURE_TRUE(shell, NS_OK);

    nsIViewManager::UpdateViewBatch batch(shell->GetViewManager());
    if (!mPrintPreviewZoomed) {
      mOriginalPrintPreviewScale = pc->GetPrintPreviewScale();
      mPrintPreviewZoomed = PR_TRUE;
    }

    mPrintPreviewZoom = aFullZoom;
    pc->SetPrintPreviewScale(aFullZoom * mOriginalPrintPreviewScale);
    nsIPageSequenceFrame* pf = shell->GetPageSequenceFrame();
    if (pf) {
      nsIFrame* f = do_QueryFrame(pf);
      shell->FrameNeedsReflow(f, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }

    nsIFrame* rootFrame = shell->GetRootFrame();
    if (rootFrame) {
      nsRect rect(nsPoint(0, 0), rootFrame->GetSize());
      rootFrame->Invalidate(rect);
    }
    batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
    return NS_OK;
  }
#endif

  mPageZoom = aFullZoom;

  nsIViewManager::UpdateViewBatch batch(GetViewManager());

  struct ZoomInfo ZoomInfo = { aFullZoom };
  CallChildren(SetChildFullZoom, &ZoomInfo);

  nsPresContext* pc = GetPresContext();
  if (pc) {
    pc->SetFullZoom(aFullZoom);
  }

  // And do the external resources
  mDocument->EnumerateExternalResources(SetExtResourceFullZoom, &ZoomInfo);

  batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);

  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetFullZoom(float* aFullZoom)
{
  NS_ENSURE_ARG_POINTER(aFullZoom);
#ifdef NS_PRINT_PREVIEW
  if (GetIsPrintPreview()) {
    *aFullZoom = mPrintPreviewZoom;
    return NS_OK;
  }
#endif
  // Check the prescontext first because it might have a temporary
  // setting for print-preview
  nsPresContext* pc = GetPresContext();
  *aFullZoom = pc ? pc->GetFullZoom() : mPageZoom;
  return NS_OK;
}

static void
SetChildAuthorStyleDisabled(nsIMarkupDocumentViewer* aChild, void* aClosure)
{
  PRBool styleDisabled  = *static_cast<PRBool*>(aClosure);
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
  const nsACString* charset = static_cast<nsACString*>(aClosure);
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
  const nsACString* charset = static_cast<nsACString*>(aClosure);
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
  const nsACString* charset = static_cast<nsACString*>(aClosure);
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
  const nsACString* charset = static_cast<nsACString*>(aClosure);
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

   // Skip doing this on docshell-less documents for now
   nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryReferent(mContainer));
   NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_NOT_AVAILABLE);
   
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
    nsCOMPtr<nsIRenderingContext> rcx =
      presShell->GetReferenceRenderingContext();
    NS_ENSURE_TRUE(rcx, NS_ERROR_FAILURE);
    prefWidth = root->GetPrefWidth(rcx);
  }

  nsresult rv = presShell->ResizeReflow(prefWidth, NS_UNCONSTRAINEDSIZE);
  NS_ENSURE_SUCCESS(rv, rv);

   nsRefPtr<nsPresContext> presContext;
   GetPresContext(getter_AddRefs(presContext));
   NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

   PRInt32 width, height;

   // so how big is it?
   nsRect shellArea = presContext->GetVisibleArea();
   // Protect against bogus returns here
   NS_ENSURE_TRUE(shellArea.width != NS_UNCONSTRAINEDSIZE &&
                  shellArea.height != NS_UNCONSTRAINEDSIZE,
                  NS_ERROR_FAILURE);
   width = presContext->AppUnitsToDevPixels(shellArea.width);
   height = presContext->AppUnitsToDevPixels(shellArea.height);

   nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
   docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
   NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

   /* presContext's size was calculated in app units and has already been
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

  *aNode = nsnull;

  // get the document
  nsIDocument* document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  // get the private dom window
  nsCOMPtr<nsPIDOMWindow> window(document->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);
  if (window) {
    nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

    // get the popup node
    nsCOMPtr<nsIDOMNode> node = root->GetPopupNode();
#ifdef MOZ_XUL
    if (!node) {
      nsPIDOMWindow* rootWindow = root->GetWindow();
      if (rootWindow) {
        nsCOMPtr<nsIDocument> rootDoc = do_QueryInterface(rootWindow->GetExtantDocument());
        if (rootDoc) {
          nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
          if (pm) {
            node = pm->GetLastTriggerPopupNode(rootDoc);
          }
        }
      }
    }
#endif
    node.swap(*aNode);
  }

  return NS_OK;
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
    nsIDocument* theDoc = mDocViewer->GetDocument();
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

  if (!mContainer) {
    PR_PL(("Container was destroyed yet we are still trying to use it!"));
    return NS_ERROR_FAILURE;
  }

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
  if (!presShell || !mDocument || !mDeviceContext) {
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
                                  float(mDeviceContext->AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom,
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

NS_IMETHODIMP
DocumentViewerImpl::PrintPreview(nsIPrintSettings* aPrintSettings, 
                                 nsIDOMWindow *aChildDOMWin, 
                                 nsIWebProgressListener* aWebProgressListener)
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  NS_WARN_IF_FALSE(IsInitializedForPrintPreview(),
                   "Using docshell.printPreview is the preferred way for print previewing!");

  NS_ENSURE_ARG_POINTER(aChildDOMWin);
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
  if (!docShell || !mDeviceContext) {
    PR_PL(("Can't Print Preview without device context and docshell"));
    return NS_ERROR_FAILURE;
  }

  if (!mPrintEngine) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aChildDOMWin->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    NS_ENSURE_STATE(doc);

    mPrintEngine = new nsPrintEngine();
    NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_OUT_OF_MEMORY);

    rv = mPrintEngine->Initialize(this, docShell, doc,
                                  float(mDeviceContext->AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom,
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
  mPrintPreviewZoomed = PR_FALSE;
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

  nsIScrollableFrame* sf =
    mPrintEngine->GetPrintPreviewPresShell()->GetRootScrollFrameAsScrollable();
  if (!sf)
    return NS_OK;

  // Check to see if we can short circut scrolling to the top
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_HOME ||
      (aType == nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM && aPageNum == 1)) {
    sf->ScrollTo(nsPoint(0, 0), nsIScrollableFrame::INSTANT);
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
  nsPoint pt = sf->GetScrollPosition();

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
    if (pageRect.Contains(pageRect.x, pt.y)) {
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

  if (fndPageFrame) {
    nscoord deadSpaceGapTwips = 0;
    nsIPageSequenceFrame * sqf = do_QueryFrame(seqFrame);
    if (sqf) {
      sqf->GetDeadSpaceValue(&deadSpaceGapTwips);
    }

    nscoord deadSpaceGap = nsPresContext::CSSTwipsToAppUnits(deadSpaceGapTwips);
    nscoord newYPosn =
      nscoord(mPrintEngine->GetPrintPreviewScale() * 
              float(fndPageFrame->GetPosition().y - deadSpaceGap));
    sf->ScrollTo(nsPoint(pt.x, newYPosn), nsIScrollableFrame::INSTANT);
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
  if (GetIsPrinting())
    return NS_ERROR_FAILURE;
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
    if (docShellTreeNode) {
      SetIsPrintingInDocShellTree(docShellTreeNode, aIsPrinting, PR_TRUE);
    } else {
      NS_WARNING("Bug 549251 Did you close a window while printing?");
    }
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
  if (!aIsPrintPreview) {
    if (mPresShell) {
      DestroyPresShell();
    }
    mWindow = nsnull;
    mViewManager = nsnull;
    mPresContext = nsnull;
    mPresShell = nsnull;
  }
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

  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  ResetFocusState(docShell);

  SetTextZoom(mTextZoom);
  SetFullZoom(mPageZoom);
  Show();

#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

//------------------------------------------------------------
// Reset ESM focus for all descendent doc shells.
static void
ResetFocusState(nsIDocShell* aDocShell)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return;

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  aDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                   nsIDocShell::ENUMERATE_FORWARDS,
                                   getter_AddRefs(docShellEnumerator));
  
  nsCOMPtr<nsISupports> currentContainer;
  PRBool hasMoreDocShells;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
         && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    nsCOMPtr<nsIDOMWindow> win = do_GetInterface(currentContainer);
    if (win)
      fm->ClearFocus(win);
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
    }
  }
#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

NS_IMETHODIMP DocumentViewerImpl::SetPageMode(PRBool aPageMode, nsIPrintSettings* aPrintSettings)
{
  // XXX Page mode is only partially working; it's currently used for
  // reftests that require a paginated context
  mIsPageMode = aPageMode;

  if (mPresShell) {
    DestroyPresShell();
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
    mPresContext = CreatePresContext(mDocument,
        nsPresContext::eContext_PageLayout, FindContainerView());
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);
    mPresContext->SetPaginatedScrolling(PR_TRUE);
    mPresContext->SetPrintSettings(aPrintSettings);
    nsresult rv = mPresContext->Init(mDeviceContext);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  InitInternal(mParentWidget, nsnull, mBounds, PR_TRUE, PR_FALSE);

  Show();
  return NS_OK;
}

NS_IMETHODIMP
DocumentViewerImpl::GetHistoryEntry(nsISHEntry **aHistoryEntry)
{
  NS_IF_ADDREF(*aHistoryEntry = mSHEntry);
  return NS_OK;
}

void
DocumentViewerImpl::DestroyPresShell()
{
  // Break circular reference (or something)
  mPresShell->EndObservingDocument();

  nsCOMPtr<nsISelection> selection;
  GetDocumentSelection(getter_AddRefs(selection));
  nsCOMPtr<nsISelectionPrivate> selPrivate = do_QueryInterface(selection);
  if (selPrivate && mSelectionListener)
    selPrivate->RemoveSelectionListener(mSelectionListener);

  nsAutoScriptBlocker scriptBlocker;
  mPresShell->Destroy();
  mPresShell = nsnull;
}

PRBool
DocumentViewerImpl::IsInitializedForPrintPreview()
{
  return mInitializedForPrintPreview;
}

void
DocumentViewerImpl::InitializeForPrintPreview()
{
  mInitializedForPrintPreview = PR_TRUE;
}

void
DocumentViewerImpl::SetPrintPreviewPresentation(nsIViewManager* aViewManager,
                                                nsPresContext* aPresContext,
                                                nsIPresShell* aPresShell)
{
  if (mPresShell) {
    DestroyPresShell();
  }

  mWindow = nsnull;
  mViewManager = aViewManager;
  mPresContext = aPresContext;
  mPresShell = aPresShell;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* container for a document and its presentation */

#include "mozilla/ServoStyleSet.h"
#include "nsAutoPtr.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewerPrint.h"
#include "nsIDOMBeforeUnloadEvent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsIFrame.h"
#include "nsIWritablePropertyBag2.h"
#include "nsSubDocumentFrame.h"

#include "nsILinkHandler.h"
#include "nsIDOMDocument.h"
#include "nsISelectionListener.h"
#include "mozilla/dom/Selection.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsContentUtils.h"
#include "nsLayoutStylesheetCache.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessible.h"
#endif
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

#include "nsViewManager.h"
#include "nsView.h"

#include "nsIPageSequenceFrame.h"
#include "nsNetUtil.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewerFile.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDocShell.h"
#include "nsIBaseWindow.h"
#include "nsILayoutHistoryState.h"
#include "nsCharsetSource.h"
#include "mozilla/ReflowInput.h"
#include "nsIImageLoadingContent.h"
#include "nsCopySupport.h"
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLImageElement.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#include "nsXULPopupManager.h"
#endif

#include "nsIClipboardHelper.h"

#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsDOMNavigationTiming.h"
#include "nsPIWindowRoot.h"
#include "nsJSEnvironment.h"
#include "nsFocusManager.h"

#include "nsIScrollableFrame.h"
#include "nsStyleSheetService.h"
#include "nsRenderingContext.h"
#include "nsILoadContext.h"

#include "nsIPrompt.h"
#include "imgIContainer.h" // image animation mode constants

#include "nsSandboxFlags.h"

#include "mozilla/DocLoadingTimelineMarker.h"

//--------------------------
// Printing Include
//---------------------------
#ifdef NS_PRINTING

#include "nsIWebBrowserPrint.h"

#include "nsPrintEngine.h"

// Print Options
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsISimpleEnumerator.h"

#include "nsIPluginDocument.h"

#endif // NS_PRINTING

//focus
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsISelectionController.h"

#include "mozilla/EventDispatcher.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIWebNavigation.h"
#include "mozilla/dom/XMLHttpRequestMainThread.h"

//paint forcing
#include <stdio.h>

#include "mozilla/dom/Element.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::dom;

#define BEFOREUNLOAD_DISABLED_PREFNAME "dom.disable_beforeunload"
#define BEFOREUNLOAD_REQUIRES_INTERACTION_PREFNAME "dom.require_user_interaction_for_beforeunload"

//-----------------------------------------------------
// LOGGING
#include "LayoutLogging.h"
#include "mozilla/Logging.h"

#ifdef NS_PRINTING
static mozilla::LazyLogModule gPrintingLog("printing");

#define PR_PL(_p1)  MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);
#endif // NS_PRINTING

#define PRT_YESNO(_p) ((_p)?"YES":"NO")
//-----------------------------------------------------

class nsDocumentViewer;
namespace mozilla {
class AutoPrintEventDispatcher;
}

// a small delegate class used to avoid circular references

class nsDocViewerSelectionListener : public nsISelectionListener
{
public:

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsISelectionListerner interface
  NS_DECL_NSISELECTIONLISTENER

                       nsDocViewerSelectionListener()
                       : mDocViewer(nullptr)
                       , mSelectionWasCollapsed(true)
                       {
                       }

  nsresult             Init(nsDocumentViewer *aDocViewer);

protected:

  virtual              ~nsDocViewerSelectionListener() {}

  nsDocumentViewer*    mDocViewer;
  bool                 mSelectionWasCollapsed;

};


/** editor Implementation of the FocusListener interface
 */
class nsDocViewerFocusListener : public nsIDOMEventListener
{
public:
  /** default constructor
   */
  nsDocViewerFocusListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult             Init(nsDocumentViewer *aDocViewer);

protected:
  /** default destructor
   */
  virtual ~nsDocViewerFocusListener();

private:
    nsDocumentViewer*  mDocViewer;
};


//-------------------------------------------------------------
class nsDocumentViewer final : public nsIContentViewer,
                               public nsIContentViewerEdit,
                               public nsIContentViewerFile,
                               public nsIDocumentViewerPrint

#ifdef NS_PRINTING
                             , public nsIWebBrowserPrint
#endif

{
  friend class nsDocViewerSelectionListener;
  friend class nsPagePrintTimer;
  friend class nsPrintEngine;

public:
  nsDocumentViewer();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

  // nsIContentViewerFile
  NS_DECL_NSICONTENTVIEWERFILE

#ifdef NS_PRINTING
  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT
#endif

  typedef void (*CallChildFunc)(nsIContentViewer* aViewer, void* aClosure);
  void CallChildren(CallChildFunc aFunc, void* aClosure);

  // nsIDocumentViewerPrint Printing Methods
  NS_DECL_NSIDOCUMENTVIEWERPRINT

protected:
  virtual ~nsDocumentViewer();

private:
  /**
   * Creates a view manager, root view, and widget for the root view, setting
   * mViewManager and mWindow.
   * @param aSize the initial size in appunits
   * @param aContainerView the container view to hook our root view up
   * to as a child, or null if this will be the root view manager
   */
  nsresult MakeWindow(const nsSize& aSize, nsView* aContainerView);

  /**
   * Create our device context
   */
  nsresult CreateDeviceContext(nsView* aContainerView);

  /**
   * If aDoCreation is true, this creates the device context, creates a
   * prescontext if necessary, and calls MakeWindow.
   *
   * If aForceSetNewDocument is false, then SetNewDocument won't be
   * called if the window's current document is already mDocument.
   */
  nsresult InitInternal(nsIWidget* aParentWidget,
                        nsISupports *aState,
                        const nsIntRect& aBounds,
                        bool aDoCreation,
                        bool aNeedMakeCX = true,
                        bool aForceSetNewDocument = true);
  /**
   * @param aDoInitialReflow set to true if you want to kick off the initial
   * reflow
   */
  nsresult InitPresentationStuff(bool aDoInitialReflow);

  nsresult GetPopupNode(nsIDOMNode** aNode);
  nsresult GetPopupLinkNode(nsIDOMNode** aNode);
  nsresult GetPopupImageNode(nsIImageLoadingContent** aNode);

  nsresult GetContentSizeInternal(int32_t* aWidth, int32_t* aHeight,
                                  nscoord aMaxWidth, nscoord aMaxHeight);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

  mozilla::dom::Selection* GetDocumentSelection();

  void DestroyPresShell();
  void DestroyPresContext();

#ifdef NS_PRINTING
  // Called when the DocViewer is notified that the state
  // of Printing or PP has changed
  void SetIsPrintingInDocShellTree(nsIDocShellTreeItem* aParentNode, 
                                   bool                 aIsPrintingOrPP, 
                                   bool                 aStartAtTop);
#endif // NS_PRINTING

  // Whether we should attach to the top level widget. This is true if we
  // are sharing/recycling a single base widget and not creating multiple
  // child widgets.
  bool ShouldAttachToTopLevel();

protected:
  // These return the current shell/prescontext etc.
  nsIPresShell* GetPresShell();
  nsPresContext* GetPresContext();
  nsViewManager* GetViewManager();

  void DetachFromTopLevelWidget();

  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  WeakPtr<nsDocShell> mContainer; // it owns me!
  nsWeakPtr mTopContainerWhilePrinting;
  RefPtr<nsDeviceContext> mDeviceContext;  // We create and own this baby

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<nsIDocument>    mDocument;
  nsCOMPtr<nsIWidget>      mWindow;      // may be null
  RefPtr<nsViewManager> mViewManager;
  RefPtr<nsPresContext>  mPresContext;
  nsCOMPtr<nsIPresShell>   mPresShell;

  nsCOMPtr<nsISelectionListener> mSelectionListener;
  RefPtr<nsDocViewerFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;
  nsCOMPtr<nsISHEntry> mSHEntry;

  nsIWidget* mParentWidget; // purposely won't be ref counted.  May be null
  bool mAttachedToParent; // view is attached to the parent widget

  nsIntRect mBounds;

  // mTextZoom/mPageZoom record the textzoom/pagezoom of the first (galley)
  // presshell only.
  float mTextZoom;      // Text zoom, defaults to 1.0
  float mPageZoom;
  float mOverrideDPPX;  // DPPX overrided, defaults to 0.0
  int mMinFontSize;

  int16_t mNumURLStarts;
  int16_t mDestroyRefCount;    // a second "refcount" for the document viewer's "destroy"

  unsigned      mStopped : 1;
  unsigned      mLoaded : 1;
  unsigned      mDeferredWindowClose : 1;
  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  unsigned      mIsSticky : 1;
  unsigned      mInPermitUnload : 1;
  unsigned      mInPermitUnloadPrompt: 1;

#ifdef NS_PRINTING
  unsigned      mClosingWhilePrinting : 1;

#if NS_PRINT_PREVIEW
  unsigned                         mPrintPreviewZoomed : 1;

  // These data members support delayed printing when the document is loading
  unsigned                         mPrintIsPending : 1;
  unsigned                         mPrintDocIsFullyLoaded : 1;
  nsCOMPtr<nsIPrintSettings>       mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;

  RefPtr<nsPrintEngine>          mPrintEngine;
  float                            mOriginalPrintPreviewScale;
  float                            mPrintPreviewZoom;
  nsAutoPtr<AutoPrintEventDispatcher> mAutoBeforeAndAfterPrint;
#endif // NS_PRINT_PREVIEW

#ifdef DEBUG
  FILE* mDebugFile;
#endif // DEBUG
#endif // NS_PRINTING

  /* character set member data */
  int32_t mHintCharsetSource;
  nsCString mHintCharset;
  nsCString mForceCharacterSet;
  
  bool mIsPageMode;
  bool mInitializedForPrintPreview;
  bool mHidden;
};

namespace mozilla {

/**
 * A RAII class for automatic dispatch of the 'beforeprint' and 'afterprint'
 * events ('beforeprint' on construction, 'afterprint' on destruction).
 *
 * https://developer.mozilla.org/en-US/docs/Web/Events/beforeprint
 * https://developer.mozilla.org/en-US/docs/Web/Events/afterprint
 */
class AutoPrintEventDispatcher
{
public:
  explicit AutoPrintEventDispatcher(nsIDocument* aTop) : mTop(aTop)
  {
    DispatchEventToWindowTree(NS_LITERAL_STRING("beforeprint"));
  }
  ~AutoPrintEventDispatcher()
  {
    DispatchEventToWindowTree(NS_LITERAL_STRING("afterprint"));
  }

private:
  void DispatchEventToWindowTree(const nsAString& aEvent)
  {
    nsCOMArray<nsIDocument> targets;
    CollectDocuments(mTop, &targets);
    for (int32_t i = 0; i < targets.Count(); ++i) {
      nsIDocument* d = targets[i];
      nsContentUtils::DispatchTrustedEvent(d, d->GetWindow(),
                                           aEvent, false, false, nullptr);
    }
  }

  static bool CollectDocuments(nsIDocument* aDocument, void* aData)
  {
    if (aDocument) {
      static_cast<nsCOMArray<nsIDocument>*>(aData)->AppendObject(aDocument);
      aDocument->EnumerateSubDocuments(CollectDocuments, aData);
    }
    return true;
  }

  nsCOMPtr<nsIDocument> mTop;
};

}

class nsDocumentShownDispatcher : public Runnable
{
public:
  explicit nsDocumentShownDispatcher(nsCOMPtr<nsIDocument> aDocument)
  : mDocument(aDocument) {}

  NS_IMETHOD Run() override;

private:
  nsCOMPtr<nsIDocument> mDocument;
};


//------------------------------------------------------------------
// nsDocumentViewer
//------------------------------------------------------------------

//------------------------------------------------------------------
already_AddRefed<nsIContentViewer>
NS_NewContentViewer()
{
  RefPtr<nsDocumentViewer> viewer = new nsDocumentViewer();
  return viewer.forget();
}

void nsDocumentViewer::PrepareToStartLoad()
{
  mStopped          = false;
  mLoaded           = false;
  mAttachedToParent = false;
  mDeferredWindowClose = false;

#ifdef NS_PRINTING
  mPrintIsPending        = false;
  mPrintDocIsFullyLoaded = false;
  mClosingWhilePrinting  = false;

  // Make sure we have destroyed it and cleared the data member
  if (mPrintEngine) {
    mPrintEngine->Destroy();
    mPrintEngine = nullptr;
#ifdef NS_PRINT_PREVIEW
    SetIsPrintPreview(false);
#endif
  }

#ifdef DEBUG
  mDebugFile = nullptr;
#endif

#endif // NS_PRINTING
}

// Note: operator new zeros our memory, so no need to init things to null.
nsDocumentViewer::nsDocumentViewer()
  : mTextZoom(1.0), mPageZoom(1.0), mOverrideDPPX(0.0), mMinFontSize(0),
    mIsSticky(true),
#ifdef NS_PRINT_PREVIEW
    mPrintPreviewZoom(1.0),
#endif
    mHintCharsetSource(kCharsetUninitialized),
    mInitializedForPrintPreview(false),
    mHidden(false)
{
  PrepareToStartLoad();
}

NS_IMPL_ADDREF(nsDocumentViewer)
NS_IMPL_RELEASE(nsDocumentViewer)

NS_INTERFACE_MAP_BEGIN(nsDocumentViewer)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewer)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerFile)
    NS_INTERFACE_MAP_ENTRY(nsIContentViewerEdit)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentViewerPrint)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentViewer)
#ifdef NS_PRINTING
    NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPrint)
#endif
NS_INTERFACE_MAP_END

nsDocumentViewer::~nsDocumentViewer()
{
  if (mDocument) {
    Close(nullptr);
    mDocument->Destroy();
  }

  NS_ASSERTION(!mPresShell && !mPresContext,
               "User did not call nsIContentViewer::Destroy");
  if (mPresShell || mPresContext) {
    // Make sure we don't hand out a reference to the content viewer to
    // the SHEntry!
    mSHEntry = nullptr;

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
/* virtual */ void
nsDocumentViewer::LoadStart(nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  if (!mDocument) {
    mDocument = aDocument;
  }
}

nsresult
nsDocumentViewer::SyncParentSubDocMap()
{
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (!docShell) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin(docShell->GetWindow());
  if (!mDocument || !pwin) {
    return NS_OK;
  }

  nsCOMPtr<Element> element = pwin->GetFrameElementInternal();
  if (!element) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShell->GetParent(getter_AddRefs(parent));

  nsCOMPtr<nsPIDOMWindowOuter> parent_win = parent ? parent->GetWindow() : nullptr;
  if (!parent_win) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> parent_doc = parent_win->GetDoc();
  if (!parent_doc) {
    return NS_OK;
  }

  if (mDocument &&
      parent_doc->GetSubDocumentFor(element) != mDocument &&
      parent_doc->EventHandlingSuppressed()) {
    mDocument->SuppressEventHandling(nsIDocument::eEvents,
                                     parent_doc->EventHandlingSuppressed());
  }
  return parent_doc->SetSubDocumentFor(element, mDocument);
}

NS_IMETHODIMP
nsDocumentViewer::SetContainer(nsIDocShell* aContainer)
{
  mContainer = static_cast<nsDocShell*>(aContainer);
  if (mPresContext) {
    mPresContext->SetContainer(mContainer);
  }

  // We're loading a new document into the window where this document
  // viewer lives, sync the parent document's frame element -> sub
  // document map

  return SyncParentSubDocMap();
}

NS_IMETHODIMP
nsDocumentViewer::GetContainer(nsIDocShell** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   nsCOMPtr<nsIDocShell> container(mContainer);
   container.swap(*aResult);
   return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Init(nsIWidget* aParentWidget,
                         const nsIntRect& aBounds)
{
  return InitInternal(aParentWidget, nullptr, aBounds, true);
}

nsresult
nsDocumentViewer::InitPresentationStuff(bool aDoInitialReflow)
{
  if (GetIsPrintPreview())
    return NS_OK;

  NS_ASSERTION(!mPresShell,
               "Someone should have destroyed the presshell!");

  // Create the style set...
  StyleSetHandle styleSet = CreateStyleSet(mDocument);

  // Now make the shell for the document
  mPresShell = mDocument->CreateShell(mPresContext, mViewManager, styleSet);
  if (!mPresShell) {
    styleSet->Delete();
    return NS_ERROR_FAILURE;
  }

  // We're done creating the style set
  styleSet->EndUpdate();

  if (aDoInitialReflow) {
    // Since Initialize() will create frames for *all* items
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
  int32_t p2a = mPresContext->AppUnitsPerDevPixel();
  MOZ_ASSERT(p2a ==
             mPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());
  nscoord width = p2a * mBounds.width;
  nscoord height = p2a * mBounds.height;

  mViewManager->SetWindowDimensions(width, height);
  mPresContext->SetTextZoom(mTextZoom);
  mPresContext->SetFullZoom(mPageZoom);
  mPresContext->SetOverrideDPPX(mOverrideDPPX);
  mPresContext->SetBaseMinFontSize(mMinFontSize);

  p2a = mPresContext->AppUnitsPerDevPixel();  // zoom may have changed it
  width = p2a * mBounds.width;
  height = p2a * mBounds.height;
  if (aDoInitialReflow) {
    nsCOMPtr<nsIPresShell> shell = mPresShell;
    // Initial reflow
    shell->Initialize(width, height);
  } else {
    // Store the visible area so it's available for other callers of
    // Initialize, like nsContentSink::StartLayout.
    mPresContext->SetVisibleArea(nsRect(0, 0, width, height));
  }

  // now register ourselves as a selection listener, so that we get
  // called when the selection changes in the window
  if (!mSelectionListener) {
    nsDocViewerSelectionListener *selectionListener =
      new nsDocViewerSelectionListener();

    selectionListener->Init(this);

    // mSelectionListener is a owning reference
    mSelectionListener = selectionListener;
  }

  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = selection->AddSelectionListener(mSelectionListener);
  if (NS_FAILED(rv))
    return rv;

  // Save old listener so we can unregister it
  RefPtr<nsDocViewerFocusListener> oldFocusListener = mFocusListener;

  // focus listener
  //
  // now register ourselves as a focus listener, so that we get called
  // when the focus changes in the window
  nsDocViewerFocusListener *focusListener = new nsDocViewerFocusListener();

  focusListener->Init(this);

  // mFocusListener is a strong reference
  mFocusListener = focusListener;

  if (mDocument) {
    mDocument->AddEventListener(NS_LITERAL_STRING("focus"),
                                mFocusListener,
                                false, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("blur"),
                                mFocusListener,
                                false, false);

    if (oldFocusListener) {
      mDocument->RemoveEventListener(NS_LITERAL_STRING("focus"),
                                     oldFocusListener, false);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("blur"),
                                     oldFocusListener, false);
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
                  nsView* aContainerView)
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
nsDocumentViewer::InitInternal(nsIWidget* aParentWidget,
                                 nsISupports *aState,
                                 const nsIntRect& aBounds,
                                 bool aDoCreation,
                                 bool aNeedMakeCX /*= true*/,
                                 bool aForceSetNewDocument /* = true*/)
{
  if (mIsPageMode) {
    // XXXbz should the InitInternal in SetPageMode just pass false
    // here itself?
    aForceSetNewDocument = false;
  }

  // We don't want any scripts to run here. That can cause flushing,
  // which can cause reentry into initialization of this document viewer,
  // which would be disastrous.
  nsAutoScriptBlocker blockScripts;

  mParentWidget = aParentWidget; // not ref counted
  mBounds = aBounds;

  nsresult rv = NS_OK;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NULL_POINTER);

  nsView* containerView = FindContainerView();

  bool makeCX = false;
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
        mPresContext = nullptr;
        return rv;
      }

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
      makeCX = !GetIsPrintPreview() && aNeedMakeCX; // needs to be true except when we are already in PP or we are enabling/disabling paginated mode.
#else
      makeCX = true;
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
        double pageWidth = 0, pageHeight = 0;
        mPresContext->GetPrintSettings()->GetEffectivePageSize(&pageWidth,
                                                               &pageHeight);
        mPresContext->SetPageSize(
          nsSize(mPresContext->CSSTwipsToAppUnits(NSToIntFloor(pageWidth)),
                 mPresContext->CSSTwipsToAppUnits(NSToIntFloor(pageHeight))));
        mPresContext->SetIsRootPaginatedDocument(true);
        mPresContext->SetPageScale(1.0f);
      }
#endif
    } else {
      // Avoid leaking the old viewer.
      if (mPreviousViewer) {
        mPreviousViewer->Destroy();
        mPreviousViewer = nullptr;
      }
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> requestor(mContainer);
  if (requestor) {
    if (mPresContext) {
      nsCOMPtr<nsILinkHandler> linkHandler;
      requestor->GetInterface(NS_GET_IID(nsILinkHandler),
                              getter_AddRefs(linkHandler));

      mPresContext->SetContainer(mContainer);
      mPresContext->SetLinkHandler(linkHandler);
    }

    // Set script-context-owner in the document

    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(requestor);

    if (window) {
      nsCOMPtr<nsIDocument> curDoc = window->GetExtantDoc();
      if (aForceSetNewDocument || curDoc != mDocument) {
        rv = window->SetNewDocument(mDocument, aState, false);
        if (NS_FAILED(rv)) {
          Destroy();
          return rv;
        }
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

void nsDocumentViewer::SetNavigationTiming(nsDOMNavigationTiming* timing)
{
  NS_ASSERTION(mDocument, "Must have a document to set navigation timing.");
  if (mDocument) {
    mDocument->SetNavigationTiming(timing);
  }
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
nsDocumentViewer::LoadComplete(nsresult aStatus)
{
  /* We need to protect ourself against auto-destruction in case the
     window is closed while processing the OnLoad event.  See bug
     http://bugzilla.mozilla.org/show_bug.cgi?id=78445 for more
     explanation.
  */
  RefPtr<nsDocumentViewer> kungFuDeathGrip(this);

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
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();

  mLoaded = true;

  // Now, fire either an OnLoad or OnError event to the document...
  bool restoring = false;
  // XXXbz imagelib kills off the document load for a full-page image with
  // NS_ERROR_PARSED_DATA_CACHED if it's in the cache.  So we want to treat
  // that one as a success code; otherwise whether we fire onload for the image
  // will depend on whether it's cached!
  if(window &&
     (NS_SUCCEEDED(aStatus) || aStatus == NS_ERROR_PARSED_DATA_CACHED)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetEvent event(true, eLoad);
    event.mFlags.mBubbles = false;
    event.mFlags.mCancelable = false;
     // XXX Dispatching to |window|, but using |document| as the target.
    event.mTarget = mDocument;

    // If the document presentation is being restored, we don't want to fire
    // onload to the document content since that would likely confuse scripts
    // on the page.

    nsIDocShell *docShell = window->GetDocShell();
    NS_ENSURE_TRUE(docShell, NS_ERROR_UNEXPECTED);

    docShell->GetRestoringDocument(&restoring);
    if (!restoring) {
      NS_ASSERTION(mDocument->IsXULDocument() || // readyState for XUL is bogus
                   mDocument->GetReadyStateEnum() ==
                     nsIDocument::READYSTATE_INTERACTIVE ||
                   // test_stricttransportsecurity.html has old-style
                   // docshell-generated about:blank docs reach this code!
                   (mDocument->GetReadyStateEnum() ==
                      nsIDocument::READYSTATE_UNINITIALIZED &&
                    NS_IsAboutBlank(mDocument->GetDocumentURI())),
                   "Bad readystate");
      nsCOMPtr<nsIDocument> d = mDocument;
      mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);

      RefPtr<nsDOMNavigationTiming> timing(d->GetNavigationTiming());
      if (timing) {
        timing->NotifyLoadEventStart();
      }

      // Dispatch observer notification to notify observers document load is complete.
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        nsIPrincipal *principal = d->NodePrincipal();
        os->NotifyObservers(d,
                            nsContentUtils::IsSystemPrincipal(principal) ?
                            "chrome-document-loaded" :
                            "content-document-loaded",
                            nullptr);
      }

      // Notify any devtools about the load.
      RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

      if (timelines && timelines->HasConsumer(docShell)) {
        timelines->AddMarkerForDocShell(docShell,
          MakeUnique<DocLoadingTimelineMarker>("document::Load"));
      }

      EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
      if (timing) {
        timing->NotifyLoadEventEnd();
      }
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
      bool isInUnload;
      if (docShell && NS_SUCCEEDED(docShell->GetIsInUnload(&isInUnload)) &&
          !isInUnload) {
        mDocument->OnPageShow(restoring, nullptr);
      }
    }
  }

  if (!mStopped) {
    if (mDocument) {
      mDocument->ScrollToRef();
    }

    // Now that the document has loaded, we can tell the presshell
    // to unsuppress painting.
    if (mPresShell) {
      nsCOMPtr<nsIPresShell> shell(mPresShell);
      shell->UnsuppressPainting();
      // mPresShell could have been removed now, see bug 378682/421432
      if (mPresShell) {
        mPresShell->LoadComplete();
      }
    }
  }

  nsJSContext::LoadEnd();

#ifdef NS_PRINTING
  // Check to see if someone tried to print during the load
  if (mPrintIsPending) {
    mPrintIsPending        = false;
    mPrintDocIsFullyLoaded = true;
    Print(mCachedPrintSettings, mCachedPrintWebProgressListner);
    mCachedPrintSettings           = nullptr;
    mCachedPrintWebProgressListner = nullptr;
  }
#endif

  return rv;
}

NS_IMETHODIMP
nsDocumentViewer::GetLoadCompleted(bool *aOutLoadCompleted)
{
  *aOutLoadCompleted = mLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::PermitUnload(bool *aPermitUnload)
{
  bool shouldPrompt = true;
  return PermitUnloadInternal(&shouldPrompt, aPermitUnload);
}


nsresult
nsDocumentViewer::PermitUnloadInternal(bool *aShouldPrompt,
                                       bool *aPermitUnload)
{
  AutoDontWarnAboutSyncXHR disableSyncXHRWarning;

  nsresult rv = NS_OK;
  *aPermitUnload = true;

  if (!mDocument
   || mInPermitUnload
   || mInPermitUnloadPrompt) {
    return NS_OK;
  }

  static bool sIsBeforeUnloadDisabled;
  static bool sBeforeUnloadRequiresInteraction;
  static bool sBeforeUnloadPrefsCached = false;

  if (!sBeforeUnloadPrefsCached) {
    sBeforeUnloadPrefsCached = true;
    Preferences::AddBoolVarCache(&sIsBeforeUnloadDisabled,
                                 BEFOREUNLOAD_DISABLED_PREFNAME);
    Preferences::AddBoolVarCache(&sBeforeUnloadRequiresInteraction,
                                 BEFOREUNLOAD_REQUIRES_INTERACTION_PREFNAME);
  }

  // First, get the script global object from the document...
  nsPIDOMWindowOuter* window = mDocument->GetWindow();

  if (!window) {
    // This is odd, but not fatal
    NS_WARNING("window not set for document!");
    return NS_OK;
  }

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "This is unsafe");

  // Now, fire an BeforeUnload event to the document and see if it's ok
  // to unload...
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMEvent> event;
  domDoc->CreateEvent(NS_LITERAL_STRING("beforeunloadevent"),
                      getter_AddRefs(event));
  nsCOMPtr<nsIDOMBeforeUnloadEvent> beforeUnload = do_QueryInterface(event);
  NS_ENSURE_STATE(beforeUnload);
  event->InitEvent(NS_LITERAL_STRING("beforeunload"), false, true);

  // Dispatching to |window|, but using |document| as the target.
  event->SetTarget(mDocument);
  event->SetTrusted(true);

  // In evil cases we might be destroyed while handling the
  // onbeforeunload event, don't let that happen. (see also bug#331040)
  RefPtr<nsDocumentViewer> kungFuDeathGrip(this);

  bool dialogsAreEnabled = false;
  {
    // Never permit popups from the beforeunload handler, no matter
    // how we get here.
    nsAutoPopupStatePusher popupStatePusher(openAbused, true);

    // Never permit dialogs from the beforeunload handler
    nsGlobalWindow* globalWindow = nsGlobalWindow::Cast(window);
    dialogsAreEnabled = globalWindow->AreDialogsEnabled();
    nsGlobalWindow::TemporarilyDisableDialogs disableDialogs(globalWindow);

    nsIDocument::PageUnloadingEventTimeStamp timestamp(mDocument);

    mInPermitUnload = true;
    {
      Telemetry::AutoTimer<Telemetry::HANDLE_BEFOREUNLOAD_MS> telemetryTimer;
      EventDispatcher::DispatchDOMEvent(window, nullptr, event, mPresContext,
                                        nullptr);
    }
    mInPermitUnload = false;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  nsAutoString text;
  beforeUnload->GetReturnValue(text);

  // NB: we nullcheck mDocument because it might now be dead as a result of
  // the event being dispatched.
  if (!sIsBeforeUnloadDisabled && *aShouldPrompt && dialogsAreEnabled &&
      mDocument && !(mDocument->GetSandboxFlags() & SANDBOXED_MODALS) &&
      (!sBeforeUnloadRequiresInteraction || mDocument->UserHasInteracted()) &&
      (event->WidgetEventPtr()->DefaultPrevented() || !text.IsEmpty())) {
    // Ask the user if it's ok to unload the current page

    nsCOMPtr<nsIPrompt> prompt = do_GetInterface(docShell);

    if (prompt) {
      nsCOMPtr<nsIWritablePropertyBag2> promptBag = do_QueryInterface(prompt);
      if (promptBag) {
        bool isTabModalPromptAllowed;
        GetIsTabModalPromptAllowed(&isTabModalPromptAllowed);
        promptBag->SetPropertyAsBool(NS_LITERAL_STRING("allowTabModal"),
                                     isTabModalPromptAllowed);
      }

      nsXPIDLString title, message, stayLabel, leaveLabel;
      rv  = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadTitle",
                                               title);
      nsresult tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadMessage",
                                               message);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadLeaveButton",
                                               leaveLabel);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
      tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                               "OnBeforeUnloadStayButton",
                                               stayLabel);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }

      if (NS_FAILED(rv) || !title || !message || !stayLabel || !leaveLabel) {
        NS_ERROR("Failed to get strings from dom.properties!");
        return NS_OK;
      }

      // Although the exact value is ignored, we must not pass invalid
      // bool values through XPConnect.
      bool dummy = false;
      int32_t buttonPressed = 0;
      uint32_t buttonFlags = (nsIPrompt::BUTTON_POS_0_DEFAULT |
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) |
                             (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_1));

      nsAutoSyncOperation sync(mDocument);
      mInPermitUnloadPrompt = true;
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_COUNT, 1);
      rv = prompt->ConfirmEx(title, message, buttonFlags,
                             leaveLabel, stayLabel, nullptr, nullptr,
                             &dummy, &buttonPressed);
      mInPermitUnloadPrompt = false;

      // If the prompt aborted, we tell our consumer that it is not allowed
      // to unload the page. One reason that prompts abort is that the user
      // performed some action that caused the page to unload while our prompt
      // was active. In those cases we don't want our consumer to also unload
      // the page.
      //
      // XXX: Are there other cases where prompts can abort? Is it ok to
      //      prevent unloading the page in those cases?
      if (NS_FAILED(rv)) {
        mozilla::Telemetry::Accumulate(mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_ACTION, 2);
        *aPermitUnload = false;
        return NS_OK;
      }

      // Button 0 == leave, button 1 == stay
      *aPermitUnload = (buttonPressed == 0);
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_ACTION,
        (*aPermitUnload ? 1 : 0));
      // If the user decided to go ahead, make sure not to prompt the user again
      // by toggling the internal prompting bool to false:
      if (*aPermitUnload) {
        *aShouldPrompt = false;
      }
    }
  }

  if (docShell) {
    int32_t childCount;
    docShell->GetChildCount(&childCount);

    for (int32_t i = 0; i < childCount && *aPermitUnload; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      docShell->GetChildAt(i, getter_AddRefs(item));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(item));

      if (docShell) {
        nsCOMPtr<nsIContentViewer> cv;
        docShell->GetContentViewer(getter_AddRefs(cv));

        if (cv) {
          cv->PermitUnloadInternal(aShouldPrompt, aPermitUnload);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetBeforeUnloadFiring(bool* aInEvent)
{
  *aInEvent = mInPermitUnload;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetInPermitUnload(bool* aInEvent)
{
  *aInEvent = mInPermitUnloadPrompt;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::PageHide(bool aIsUnload)
{
  AutoDontWarnAboutSyncXHR disableSyncXHRWarning;

  mHidden = true;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument->OnPageHide(!aIsUnload, nullptr);

  // inform the window so that the focus state is reset.
  NS_ENSURE_STATE(mDocument);
  nsPIDOMWindowOuter* window = mDocument->GetWindow();
  if (window)
    window->PageHidden();

  if (aIsUnload) {
    // Poke the GC. The window might be collectable garbage now.
    nsJSContext::PokeGC(JS::gcreason::PAGE_HIDE, NS_GC_DELAY * 2);

    // if Destroy() was called during OnPageHide(), mDocument is nullptr.
    NS_ENSURE_STATE(mDocument);

    // First, get the window from the document...
    nsPIDOMWindowOuter* window = mDocument->GetWindow();

    if (!window) {
      // Fail if no window is available...
      NS_WARNING("window not set for document!");
      return NS_ERROR_NULL_POINTER;
    }

    // Now, fire an Unload event to the document...
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetEvent event(true, eUnload);
    event.mFlags.mBubbles = false;
    // XXX Dispatching to |window|, but using |document| as the target.
    event.mTarget = mDocument;

    // Never permit popups from the unload handler, no matter how we get
    // here.
    nsAutoPopupStatePusher popupStatePusher(openAbused, true);

    nsIDocument::PageUnloadingEventTimeStamp timestamp(mDocument);

    {
      Telemetry::AutoTimer<Telemetry::HANDLE_UNLOAD_MS> telemetryTimer;
      EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
    }
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
  if (viewer) {
    viewer->SetIsHidden(false);
    nsIDocument* doc = viewer->GetDocument();
    if (doc) {
      doc->SetContainer(static_cast<nsDocShell*>(aShell));
    }
    RefPtr<nsPresContext> pc;
    viewer->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      pc->SetContainer(static_cast<nsDocShell*>(aShell));
      nsCOMPtr<nsILinkHandler> handler = do_QueryInterface(aShell);
      pc->SetLinkHandler(handler);
    }
    nsCOMPtr<nsIPresShell> presShell;
    viewer->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      presShell->SetForwardingContainer(WeakPtr<nsDocShell>());
    }
  }

  // Now recurse through the children
  int32_t childCount;
  aShell->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    aShell->GetChildAt(i, getter_AddRefs(childItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(childItem);
    AttachContainerRecurse(shell);
  }
}

NS_IMETHODIMP
nsDocumentViewer::Open(nsISupports *aState, nsISHEntry *aSHEntry)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);

  if (mDocument)
    mDocument->SetContainer(mContainer);

  nsresult rv = InitInternal(mParentWidget, aState, mBounds, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mHidden = false;

  if (mPresShell)
    mPresShell->SetForwardingContainer(WeakPtr<nsDocShell>());

  // Rehook the child presentations.  The child shells are still in
  // session history, so get them from there.

  if (aSHEntry) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    int32_t itemIndex = 0;
    while (NS_SUCCEEDED(aSHEntry->ChildShellAt(itemIndex++,
                                               getter_AddRefs(item))) && item) {
      nsCOMPtr<nsIDocShell> shell = do_QueryInterface(item);
      AttachContainerRecurse(shell);
    }
  }
  
  SyncParentSubDocMap();

  if (mFocusListener && mDocument) {
    mDocument->AddEventListener(NS_LITERAL_STRING("focus"), mFocusListener,
                                false, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("blur"), mFocusListener,
                                false, false);
  }

  // XXX re-enable image animations once that works correctly

  PrepareToStartLoad();

  // When loading a page from the bfcache with puppet widgets, we do the
  // widget attachment here (it is otherwise done in MakeWindow, which is
  // called for non-bfcache pages in the history, but not bfcache pages).
  // Attachment is necessary, since we get detached when another page
  // is browsed to. That is, if we are one page A, then when we go to
  // page B, we detach. So page A's view has no widget. If we then go
  // back to it, and it is in the bfcache, we will use that view, which
  // doesn't have a widget. The attach call here will properly attach us.
  if (nsIWidget::UsePuppetWidgets() && mPresContext &&
      ShouldAttachToTopLevel()) {
    // If the old view is already attached to our parent, detach
    DetachFromTopLevelWidget();

    nsViewManager *vm = GetViewManager();
    MOZ_ASSERT(vm, "no view manager");
    nsView* v = vm->GetRootView();
    MOZ_ASSERT(v, "no root view");
    MOZ_ASSERT(mParentWidget, "no mParentWidget to set");
    v->AttachToTopLevelWidget(mParentWidget);

    mAttachedToParent = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Close(nsISHEntry *aSHEntry)
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
    mPrintEngine->TurnScriptingOn(true);
  }
#endif

#ifdef NS_PRINTING
  // A Close was called while we were printing
  // so don't clear the ScriptGlobalObject
  // or clear the mDocument below
  if (mPrintEngine && !mClosingWhilePrinting) {
    mClosingWhilePrinting = true;
  } else
#endif
    {
      // out of band cleanup of docshell
      mDocument->SetScriptGlobalObject(nullptr);

      if (!mSHEntry && mDocument)
        mDocument->RemovedFromDocShell();
    }

  if (mFocusListener && mDocument) {
    mDocument->RemoveEventListener(NS_LITERAL_STRING("focus"), mFocusListener,
                                   false);
    mDocument->RemoveEventListener(NS_LITERAL_STRING("blur"), mFocusListener,
                                   false);
  }

  return NS_OK;
}

static void
DetachContainerRecurse(nsIDocShell *aShell)
{
  // Unhook this docshell's presentation
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  if (viewer) {
    nsIDocument* doc = viewer->GetDocument();
    if (doc) {
      doc->SetContainer(nullptr);
    }
    RefPtr<nsPresContext> pc;
    viewer->GetPresContext(getter_AddRefs(pc));
    if (pc) {
      pc->Detach();
    }
    nsCOMPtr<nsIPresShell> presShell;
    viewer->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      auto weakShell = static_cast<nsDocShell*>(aShell);
      presShell->SetForwardingContainer(weakShell);
    }
  }

  // Now recurse through the children
  int32_t childCount;
  aShell->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    aShell->GetChildAt(i, getter_AddRefs(childItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(childItem);
    DetachContainerRecurse(shell);
  }
}

NS_IMETHODIMP
nsDocumentViewer::Destroy()
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
  // Dispatch the 'afterprint' event now, if pending:
  mAutoBeforeAndAfterPrint = nullptr;
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
    mIsSticky = true;

    bool savePresentation = mDocument ? mDocument->IsBFCachingAllowed() : true;

    // Remove our root view from the view hierarchy.
    if (mPresShell) {
      nsViewManager *vm = mPresShell->GetViewManager();
      if (vm) {
        nsView *rootView = vm->GetRootView();

        if (rootView) {
          nsView *rootViewParent = rootView->GetParent();
          if (rootViewParent) {
            nsViewManager *parentVM = rootViewParent->GetViewManager();
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
      mDocument->Sanitize();
    }

    // Reverse ownership. Do this *after* calling sanitize so that sanitize
    // doesn't cause mutations that make the SHEntry drop the presentation

    // Grab a reference to mSHEntry before calling into things like
    // SyncPresentationState that might mess with our members.
    nsCOMPtr<nsISHEntry> shEntry = mSHEntry; // we'll need this below
    mSHEntry = nullptr;

    if (savePresentation) {
      shEntry->SetContentViewer(this);
    }

    // Always sync the presentation state.  That way even if someone screws up
    // and shEntry has no window state at this point we'll be ok; we just won't
    // cache ourselves.
    shEntry->SyncPresentationState();

    // Shut down accessibility for the document before we start to tear it down.
#ifdef ACCESSIBILITY
    if (mPresShell) {
      a11y::DocAccessible* docAcc = mPresShell->GetDocAccessible();
      if (docAcc) {
        docAcc->Shutdown();
      }
    }
#endif

    // Break the link from the document/presentation to the docshell, so that
    // link traversals cannot affect the currently-loaded document.
    // When the presentation is restored, Open() and InitInternal() will reset
    // these pointers to their original values.

    if (mDocument) {
      mDocument->SetContainer(nullptr);
    }
    if (mPresContext) {
      mPresContext->Detach();
    }
    if (mPresShell) {
      mPresShell->SetForwardingContainer(mContainer);
    }

    // Do the same for our children.  Note that we need to get the child
    // docshells from the SHEntry now; the docshell will have cleared them.
    nsCOMPtr<nsIDocShellTreeItem> item;
    int32_t itemIndex = 0;
    while (NS_SUCCEEDED(shEntry->ChildShellAt(itemIndex++,
                                              getter_AddRefs(item))) && item) {
      nsCOMPtr<nsIDocShell> shell = do_QueryInterface(item);
      DetachContainerRecurse(shell);
    }

    return NS_OK;
  }

  // The document was not put in the bfcache

  if (mPresShell) {
    DestroyPresShell();
  }
  if (mDocument) {
    mDocument->Destroy();
    mDocument = nullptr;
  }

  // All callers are supposed to call destroy to break circular
  // references.  If we do this stuff in the destructor, the
  // destructor might never be called (especially if we're being
  // used from JS.

#ifdef NS_PRINTING
  if (mPrintEngine) {
#ifdef NS_PRINT_PREVIEW
    bool doingPrintPreview;
    mPrintEngine->GetDoingPrintPreview(&doingPrintPreview);
    if (doingPrintPreview) {
      mPrintEngine->FinishPrintPreview();
    }
#endif

    mPrintEngine->Destroy();
    mPrintEngine = nullptr;
  }
#endif

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nullptr;
  }

  mDeviceContext = nullptr;

  if (mPresContext) {
    DestroyPresContext();
  }

  mWindow = nullptr;
  mViewManager = nullptr;
  mContainer = WeakPtr<nsDocShell>();

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Stop(void)
{
  NS_ASSERTION(mDocument, "Stop called too early or too late");
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  if (!mHidden && (mLoaded || mStopped) && mPresContext && !mSHEntry)
    mPresContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);

  mStopped = true;

  if (!mLoaded && mPresShell) {
    // Well, we might as well paint what we have so far.
    nsCOMPtr<nsIPresShell> shell(mPresShell); // bug 378682
    shell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetDOMDocument(nsIDOMDocument **aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  return CallQueryInterface(mDocument, aResult);
}

NS_IMETHODIMP_(nsIDocument *)
nsDocumentViewer::GetDocument()
{
  return mDocument;
}

NS_IMETHODIMP
nsDocumentViewer::SetDOMDocument(nsIDOMDocument *aDocument)
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

  return SetDocumentInternal(newDoc, false);
}

NS_IMETHODIMP
nsDocumentViewer::SetDocumentInternal(nsIDocument* aDocument,
                                        bool aForceReuseInnerWindow)
{
  MOZ_ASSERT(aDocument);

  // Set new container
  aDocument->SetContainer(mContainer);

  if (mDocument != aDocument) {
    if (aForceReuseInnerWindow) {
      // Transfer the navigation timing information to the new document, since
      // we're keeping the same inner and hence should really have the same
      // timing information.
      aDocument->SetNavigationTiming(mDocument->GetNavigationTiming());
    }

    if (mDocument->IsStaticDocument()) {
      mDocument->Destroy();
    }

    // Clear the list of old child docshells. Child docshells for the new
    // document will be constructed as frames are created.
    if (!aDocument->IsStaticDocument()) {
      nsCOMPtr<nsIDocShell> node(mContainer);
      if (node) {
        int32_t count;
        node->GetChildCount(&count);
        for (int32_t i = 0; i < count; ++i) {
          nsCOMPtr<nsIDocShellTreeItem> child;
          node->GetChildAt(0, getter_AddRefs(child));
          node->RemoveChild(child);
        }
      }
    }

    // Replace the old document with the new one. Do this only when
    // the new document really is a new document.
    mDocument = aDocument;

    // Set the script global object on the new document
    nsCOMPtr<nsPIDOMWindowOuter> window =
      mContainer ? mContainer->GetWindow() : nullptr;
    if (window) {
      nsresult rv = window->SetNewDocument(aDocument, nullptr,
                                           aForceReuseInnerWindow);
      if (NS_FAILED(rv)) {
        Destroy();
        return rv;
      }
    }
  }

  nsresult rv = SyncParentSubDocMap();
  NS_ENSURE_SUCCESS(rv, rv);

  // Replace the current pres shell with a new shell for the new document

  if (mPresShell) {
    DestroyPresShell();
  }

  if (mPresContext) {
    DestroyPresContext();

    mWindow = nullptr;
    rv = InitInternal(mParentWidget, nullptr, mBounds, true, true, false);
  }

  return rv;
}

nsIPresShell*
nsDocumentViewer::GetPresShell()
{
  return mPresShell;
}

nsPresContext*
nsDocumentViewer::GetPresContext()
{
  return mPresContext;
}

nsViewManager*
nsDocumentViewer::GetViewManager()
{
  return mViewManager;
}

NS_IMETHODIMP
nsDocumentViewer::GetPresShell(nsIPresShell** aResult)
{
  nsIPresShell* shell = GetPresShell();
  NS_IF_ADDREF(*aResult = shell);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetPresContext(nsPresContext** aResult)
{
  nsPresContext* pc = GetPresContext();
  NS_IF_ADDREF(*aResult = pc);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetBounds(nsIntRect& aResult)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  aResult = mBounds;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetPreviousViewer(nsIContentViewer** aViewer)
{
  *aViewer = mPreviousViewer;
  NS_IF_ADDREF(*aViewer);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetPreviousViewer(nsIContentViewer* aViewer)
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
      aViewer->SetPreviousViewer(nullptr);
      aViewer->Destroy();
      return SetPreviousViewer(prevViewer);
    }
  }

  mPreviousViewer = aViewer;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetBoundsWithFlags(const nsIntRect& aBounds, uint32_t aFlags)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  mBounds = aBounds;

  if (mWindow && !mAttachedToParent) {
    // Resize the widget, but don't trigger repaint. Layout will generate
    // repaint requests during reflow.
    mWindow->Resize(aBounds.x, aBounds.y,
                    aBounds.width, aBounds.height,
                    false);
  } else if (mPresContext && mViewManager) {
    // Ensure presContext's deviceContext is up to date, as we sometimes get
    // here before a resolution-change notification has been fully handled
    // during display configuration changes, especially when there are lots
    // of windows/widgets competing to handle the notifications.
    // (See bug 1154125.)
    if (mPresContext->DeviceContext()->CheckDPIChange()) {
      mPresContext->UIResolutionChanged();
    }
    int32_t p2a = mPresContext->AppUnitsPerDevPixel();
    mViewManager->SetWindowDimensions(NSIntPixelsToAppUnits(mBounds.width, p2a),
                                      NSIntPixelsToAppUnits(mBounds.height, p2a),
                                      !!(aFlags & nsIContentViewer::eDelayResize));
  }

  // If there's a previous viewer, it's the one that's actually showing,
  // so be sure to resize it as well so it paints over the right area.
  // This may slow down the performance of the new page load, but resize
  // during load is also probably a relatively unusual condition
  // relating to things being hidden while something is loaded.  It so
  // happens that Firefox does this a good bit with its infobar, and it
  // looks ugly if we don't do this.
  if (mPreviousViewer) {
    nsCOMPtr<nsIContentViewer> previousViewer = mPreviousViewer;
    previousViewer->SetBounds(aBounds);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetBounds(const nsIntRect& aBounds)
{
  return SetBoundsWithFlags(aBounds, 0);
}

NS_IMETHODIMP
nsDocumentViewer::Move(int32_t aX, int32_t aY)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  mBounds.MoveTo(aX, aY);
  if (mWindow) {
    mWindow->Move(aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Show(void)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  // We don't need the previous viewer anymore since we're not
  // displaying it.
  if (mPreviousViewer) {
    // This little dance *may* only be to keep
    // PresShell::EndObservingDocument happy, but I'm not sure.
    nsCOMPtr<nsIContentViewer> prevViewer(mPreviousViewer);
    mPreviousViewer = nullptr;
    prevViewer->Destroy();

    // Make sure we don't have too many cached ContentViewers
    nsCOMPtr<nsIDocShellTreeItem> treeItem(mContainer);
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
        int32_t prevIndex,loadedIndex;
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(treeItem);
        docShell->GetPreviousTransIndex(&prevIndex);
        docShell->GetLoadedTransIndex(&loadedIndex);
#ifdef DEBUG_PAGE_CACHE
        printf("About to evict content viewers: prev=%d, loaded=%d\n",
               prevIndex, loadedIndex);
#endif
        historyInt->EvictOutOfRangeContentViewers(loadedIndex);
      }
    }
  }

  if (mWindow) {
    // When attached to a top level xul window, we do not need to call
    // Show on the widget. Underlying window management code handles
    // this when the window is initialized.
    if (!mAttachedToParent) {
      mWindow->Show(true);
    }
  }

  if (mDocument && !mPresShell) {
    NS_ASSERTION(!mWindow, "Window already created but no presshell?");

    nsCOMPtr<nsIBaseWindow> base_win(mContainer);
    if (base_win) {
      base_win->GetParentWidget(&mParentWidget);
      if (mParentWidget) {
        mParentWidget->Release(); // GetParentWidget AddRefs, but mParentWidget is weak
      }
    }

    nsView* containerView = FindContainerView();

    nsresult rv = CreateDeviceContext(containerView);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create presentation context
    NS_ASSERTION(!mPresContext, "Shouldn't have a prescontext if we have no shell!");
    mPresContext = CreatePresContext(mDocument,
        nsPresContext::eContext_Galley, containerView);
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

    rv = mPresContext->Init(mDeviceContext);
    if (NS_FAILED(rv)) {
      mPresContext = nullptr;
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

      mPresContext->SetContainer(mContainer);
    }

    if (mPresContext) {
      Hide();

      rv = InitPresentationStuff(mDocument->MayStartLayout());
    }

    // If we get here the document load has already started and the
    // window is shown because some JS on the page caused it to be
    // shown...

    if (mPresShell) {
      nsCOMPtr<nsIPresShell> shell(mPresShell); // bug 378682
      shell->UnsuppressPainting();
    }
  }

  // Notify observers that a new page has been shown. This will get run
  // from the event loop after we actually draw the page.
  NS_DispatchToMainThread(new nsDocumentShownDispatcher(mDocument));

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Hide(void)
{
  if (!mAttachedToParent && mWindow) {
    mWindow->Show(false);
  }

  if (!mPresShell)
    return NS_OK;

  NS_ASSERTION(mPresContext, "Can't have a presshell and no prescontext!");

  // Avoid leaking the old viewer.
  if (mPreviousViewer) {
    mPreviousViewer->Destroy();
    mPreviousViewer = nullptr;
  }

  if (mIsSticky) {
    // This window is sticky, that means that it might be shown again
    // and we don't want the presshell n' all that to be thrown away
    // just because the window is hidden.

    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell) {
#ifdef DEBUG
    nsCOMPtr<nsIContentViewer> currentViewer;
    docShell->GetContentViewer(getter_AddRefs(currentViewer));
    MOZ_ASSERT(currentViewer == this);
#endif
    nsCOMPtr<nsILayoutHistoryState> layoutState;
    mPresShell->CaptureHistoryState(getter_AddRefs(layoutState));
  }

  DestroyPresShell();

  DestroyPresContext();

  mViewManager   = nullptr;
  mWindow        = nullptr;
  mDeviceContext = nullptr;
  mParentWidget  = nullptr;

  nsCOMPtr<nsIBaseWindow> base_win(mContainer);

  if (base_win && !mAttachedToParent) {
    base_win->SetParentWidget(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetSticky(bool *aSticky)
{
  *aSticky = mIsSticky;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetSticky(bool aSticky)
{
  mIsSticky = aSticky;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::RequestWindowClose(bool* aCanClose)
{
#ifdef NS_PRINTING
  if (mPrintIsPending || (mPrintEngine && mPrintEngine->GetIsPrinting())) {
    *aCanClose = false;
    mDeferredWindowClose = true;
  } else
#endif
    *aCanClose = true;

  return NS_OK;
}

StyleSetHandle
nsDocumentViewer::CreateStyleSet(nsIDocument* aDocument)
{
  // Make sure this does the same thing as PresShell::AddSheet wrt ordering.

  // this should eventually get expanded to allow for creating
  // different sets for different media

  StyleBackendType backendType = aDocument->GetStyleBackendType();

  StyleSetHandle styleSet;
  if (backendType == StyleBackendType::Gecko) {
    styleSet = new nsStyleSet();
  } else {
    styleSet = new ServoStyleSet();
  }

  styleSet->BeginUpdate();
  
  // The document will fill in the document sheets when we create the presshell

  if (aDocument->IsBeingUsedAsImage()) {
    MOZ_ASSERT(aDocument->IsSVGDocument(),
               "Do we want to skip most sheets for this new image type?");

    // SVG-as-an-image must be kept as light and small as possible. We
    // deliberately skip loading everything and leave svg.css (and html.css and
    // xul.css) to be loaded on-demand.
    // XXXjwatt Nothing else is loaded on-demand, but I don't think that
    // should matter for SVG-as-an-image. If it does, I want to know why!

    // Caller will handle calling EndUpdate, per contract.
    return styleSet;
  }

  auto cache = nsLayoutStylesheetCache::For(backendType);

  // Handle the user sheets.
  StyleSheet* sheet = nullptr;
  if (nsContentUtils::IsInChromeDocshell(aDocument)) {
    sheet = cache->UserChromeSheet();
  } else {
    sheet = cache->UserContentSheet();
  }

  if (sheet)
    styleSet->AppendStyleSheet(SheetType::User, sheet);

  // Append chrome sheets (scrollbars + forms).
  bool shouldOverride = false;
  // We don't want a docshell here for external resource docs, so just
  // look at mContainer.
  nsCOMPtr<nsIDocShell> ds(mContainer);
  nsCOMPtr<nsIDOMEventTarget> chromeHandler;
  nsCOMPtr<nsIURI> uri;
  RefPtr<StyleSheet> chromeSheet;

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
        RefPtr<mozilla::css::Loader> cssLoader =
          new mozilla::css::Loader(backendType);

        char *str = ToNewCString(sheets);
        char *newStr = str;
        char *token;
        while ( (token = nsCRT::strtok(newStr, ", ", &newStr)) ) {
          NS_NewURI(getter_AddRefs(uri), nsDependentCString(token), nullptr,
                    baseURI);
          if (!uri) continue;

          cssLoader->LoadSheetSync(uri, &chromeSheet);
          if (!chromeSheet) continue;

          styleSet->PrependStyleSheet(SheetType::Agent, chromeSheet);
          shouldOverride = true;
        }
        free(str);
      }
    }
  }

  if (!shouldOverride) {
    sheet = cache->ScrollbarsSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }
  }

  if (!aDocument->IsSVGDocument()) {
    // !!! IMPORTANT - KEEP THIS BLOCK IN SYNC WITH
    // !!! SVGDocument::EnsureNonSVGUserAgentStyleSheetsLoaded.

    // SVGForeignObjectElement::BindToTree calls SVGDocument::
    // EnsureNonSVGUserAgentStyleSheetsLoaded to loads these UA sheet
    // on-demand. (Excluding the quirks sheet, which should never be loaded for
    // an SVG document, and excluding xul.css which will be loaded on demand by
    // nsXULElement::BindToTree.)

    sheet = cache->NumberControlSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }

    sheet = cache->FormsSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }

    if (aDocument->LoadsFullXULStyleSheetUpFront()) {
      // nsXULElement::BindToTree loads xul.css on-demand if we don't load it
      // up-front here.
      sheet = cache->XULSheet();
      if (sheet) {
        styleSet->PrependStyleSheet(SheetType::Agent, sheet);
      }
    }

    sheet = cache->MinimalXULSheet();
    if (sheet) {
      // Load the minimal XUL rules for scrollbars and a few other XUL things
      // that non-XUL (typically HTML) documents commonly use.
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }

    sheet = cache->CounterStylesSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }

    if (nsLayoutUtils::ShouldUseNoScriptSheet(aDocument)) {
      sheet = cache->NoScriptSheet();
      if (sheet) {
        styleSet->PrependStyleSheet(SheetType::Agent, sheet);
      }
    }

    if (nsLayoutUtils::ShouldUseNoFramesSheet(aDocument)) {
      sheet = cache->NoFramesSheet();
      if (sheet) {
        styleSet->PrependStyleSheet(SheetType::Agent, sheet);
      }
    }

    // We don't add quirk.css here; nsPresContext::CompatibilityModeChanged will
    // append it if needed.

    sheet = cache->HTMLSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }

    styleSet->PrependStyleSheet(SheetType::Agent,
                                cache->UASheet());
  } else {
    // SVG documents may have scrollbars and need the scrollbar styling.
    sheet = cache->MinimalXULSheet();
    if (sheet) {
      styleSet->PrependStyleSheet(SheetType::Agent, sheet);
    }
  }

  if (styleSet->IsGecko()) {
    nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
    if (sheetService) {
      for (StyleSheet* sheet : *sheetService->AgentStyleSheets()) {
        styleSet->AppendStyleSheet(SheetType::Agent, sheet);
      }
      for (StyleSheet* sheet : Reversed(*sheetService->UserStyleSheets())) {
        styleSet->PrependStyleSheet(SheetType::User, sheet);
      }
    }
  } else {
    NS_WARNING("stylo: Not yet checking nsStyleSheetService for Servo-backed "
               "documents. See bug 1290224");
  }

  // Caller will handle calling EndUpdate, per contract.
  return styleSet;
}

NS_IMETHODIMP
nsDocumentViewer::ClearHistoryEntry()
{
  mSHEntry = nullptr;
  return NS_OK;
}

//-------------------------------------------------------

nsresult
nsDocumentViewer::MakeWindow(const nsSize& aSize, nsView* aContainerView)
{
  if (GetIsPrintPreview())
    return NS_OK;

  bool shouldAttach = ShouldAttachToTopLevel();

  if (shouldAttach) {
    // If the old view is already attached to our parent, detach
    DetachFromTopLevelWidget();
  }

  mViewManager = new nsViewManager();

  nsDeviceContext *dx = mPresContext->DeviceContext();

  nsresult rv = mViewManager->Init(dx);
  if (NS_FAILED(rv))
    return rv;

  // The root view is always at 0,0.
  nsRect tbounds(nsPoint(0, 0), aSize);
  // Create a view
  nsView* view = mViewManager->CreateView(tbounds, aContainerView);
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
      initDataPtr = nullptr;
    }

    if (shouldAttach) {
      // Reuse the top level parent widget.
      rv = view->AttachToTopLevelWidget(mParentWidget);
      mAttachedToParent = true;
    }
    else if (!aContainerView && mParentWidget) {
      rv = view->CreateWidgetForParent(mParentWidget, initDataPtr,
                                       true, false);
    }
    else {
      rv = view->CreateWidget(initDataPtr, true, false);
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
nsDocumentViewer::DetachFromTopLevelWidget()
{
  if (mViewManager) {
    nsView* oldView = mViewManager->GetRootView();
    if (oldView && oldView->IsAttachedToTopLevel()) {
      oldView->DetachFromTopLevelWidget();
    }
  }
  mAttachedToParent = false;
}

nsView*
nsDocumentViewer::FindContainerView()
{
  if (!mContainer) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  nsCOMPtr<nsPIDOMWindowOuter> pwin(docShell->GetWindow());
  if (!pwin) {
    return nullptr;
  }

  nsCOMPtr<Element> containerElement = pwin->GetFrameElementInternal();
  if (!containerElement) {
    return nullptr;
  }

  nsIFrame* subdocFrame = nsLayoutUtils::GetRealPrimaryFrameFor(containerElement);
  if (!subdocFrame) {
    // XXX Silenced by default in bug 1175289
    LAYOUT_WARNING("Subdocument container has no frame");
    return nullptr;
  }

  // subdocFrame might not be a subdocument frame; the frame
  // constructor can treat a <frame> as an inline in some XBL
  // cases. Treat that as display:none, the document is not
  // displayed.
  if (subdocFrame->GetType() != nsGkAtoms::subDocumentFrame) {
    NS_WARNING_ASSERTION(!subdocFrame->GetType(),
                         "Subdocument container has non-subdocument frame");
    return nullptr;
  }

  NS_ASSERTION(subdocFrame->GetView(), "Subdoc frames must have views");
  return static_cast<nsSubDocumentFrame*>(subdocFrame)->EnsureInnerView();
}

nsresult
nsDocumentViewer::CreateDeviceContext(nsView* aContainerView)
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
  nsIWidget* widget = nullptr;
  if (aContainerView) {
    widget = aContainerView->GetNearestWidget(nullptr);
  }
  if (!widget) {
    widget = mParentWidget;
  }
  if (widget) {
    widget = widget->GetTopLevelWidget();
  }

  mDeviceContext = new nsDeviceContext();
  mDeviceContext->Init(widget);
  return NS_OK;
}

// Return the selection for the document. Note that text fields have their
// own selection, which cannot be accessed with this method.
mozilla::dom::Selection*
nsDocumentViewer::GetDocumentSelection()
{
  if (!mPresShell) {
    return nullptr;
  }

  return mPresShell->GetCurrentSelection(SelectionType::eNormal);
}

/* ========================================================================================
 * nsIContentViewerEdit
 * ======================================================================================== */

NS_IMETHODIMP nsDocumentViewer::ClearSelection()
{
  // use nsCopySupport::GetSelectionForCopy() ?
  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  return selection->CollapseToStart();
}

NS_IMETHODIMP nsDocumentViewer::SelectAll()
{
  // XXX this is a temporary implementation copied from nsWebShell
  // for now. I think nsDocument and friends should have some helper
  // functions to make this easier.

  // use nsCopySupport::GetSelectionForCopy() ?
  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMHTMLDocument> htmldoc = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMNode> bodyNode;

  nsresult rv;
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

  mozilla::dom::Selection::AutoUserInitiated userSelection(selection);
  rv = selection->SelectAllChildren(bodyNode);
  return rv;
}

NS_IMETHODIMP nsDocumentViewer::CopySelection()
{
  nsCopySupport::FireClipboardEvent(eCopy, nsIClipboard::kGlobalClipboard,
                                    mPresShell, nullptr);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::CopyLinkLocation()
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIDOMNode> node;
  GetPopupLinkNode(getter_AddRefs(node));
  // make noise if we're not in a link
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsCOMPtr<dom::Element> elm(do_QueryInterface(node));
  NS_ENSURE_TRUE(elm, NS_ERROR_FAILURE);

  nsAutoString locationText;
  nsContentUtils::GetLinkLocation(elm, locationText);
  if (locationText.IsEmpty())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIClipboardHelper> clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // copy the href onto the clipboard
  return clipboard->CopyString(locationText);
}

NS_IMETHODIMP nsDocumentViewer::CopyImage(int32_t aCopyFlags)
{
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIImageLoadingContent> node;
  GetPopupImageNode(getter_AddRefs(node));
  // make noise if we're not in an image
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadContext> loadContext(mContainer);
  return nsCopySupport::ImageCopy(node, loadContext, aCopyFlags);
}


NS_IMETHODIMP nsDocumentViewer::GetCopyable(bool *aCopyable)
{
  NS_ENSURE_ARG_POINTER(aCopyable);
  *aCopyable = nsCopySupport::CanCopy(mDocument);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetContents(const char *mimeType, bool selectionOnly, nsAString& aOutValue)
{
  aOutValue.Truncate();

  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  // Now we have the selection.  Make sure it's nonzero:
  nsCOMPtr<nsISelection> sel;
  if (selectionOnly) {
    nsCopySupport::GetSelectionForCopy(mDocument, getter_AddRefs(sel));
    NS_ENSURE_TRUE(sel, NS_ERROR_FAILURE);
  
    bool isCollapsed;
    sel->GetIsCollapsed(&isCollapsed);
    if (isCollapsed)
      return NS_OK;
  }

  // call the copy code
  return nsCopySupport::GetContents(nsDependentCString(mimeType), 0, sel,
                                    mDocument, aOutValue);
}

NS_IMETHODIMP nsDocumentViewer::GetCanGetContents(bool *aCanGetContents)
{
  NS_ENSURE_ARG_POINTER(aCanGetContents);
  *aCanGetContents = false;
  NS_ENSURE_STATE(mDocument);
  *aCanGetContents = nsCopySupport::CanCopy(mDocument);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::SetCommandNode(nsIDOMNode* aNode)
{
  nsIDocument* document = GetDocument();
  NS_ENSURE_STATE(document);

  nsCOMPtr<nsPIDOMWindowOuter> window(document->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_STATE(root);

  root->SetPopupNode(aNode);
  return NS_OK;
}

/* ========================================================================================
 * nsIContentViewerFile
 * ======================================================================================== */
/** ---------------------------------------------------
 *  See documentation above in the nsIContentViewerfile class definition
 *	@update 01/24/00 dwc
 */
NS_IMETHODIMP
nsDocumentViewer::Print(bool              aSilent,
                          FILE *            aDebugFile,
                          nsIPrintSettings* aPrintSettings)
{
#ifdef NS_PRINTING
  nsCOMPtr<nsIPrintSettings> printSettings;

#ifdef DEBUG
  nsresult rv = NS_ERROR_FAILURE;

  mDebugFile = aDebugFile;
  // if they don't pass in a PrintSettings, then make one
  // it will have all the default values
  printSettings = aPrintSettings;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc
    = do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    // if they don't pass in a PrintSettings, then make one
    if (printSettings == nullptr) {
      printSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
    }
    NS_ASSERTION(printSettings, "You can't PrintPreview without a PrintSettings!");
  }
  if (printSettings) printSettings->SetPrintSilent(aSilent);
  if (printSettings) printSettings->SetShowPrintProgress(false);
#endif


  return Print(printSettings, nullptr);
#else
  return NS_ERROR_FAILURE;
#endif
}

// nsIContentViewerFile interface
NS_IMETHODIMP
nsDocumentViewer::GetPrintable(bool *aPrintable)
{
  NS_ENSURE_ARG_POINTER(aPrintable);

  *aPrintable = !GetIsPrinting();

  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::ScrollToNode(nsIDOMNode* aNode)
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
  NS_ENSURE_SUCCESS(
    presShell->ScrollContentIntoView(content,
                                     nsIPresShell::ScrollAxis(
                                       nsIPresShell::SCROLL_TOP,
                                       nsIPresShell::SCROLL_ALWAYS),
                                     nsIPresShell::ScrollAxis(),
                                     nsIPresShell::SCROLL_OVERFLOW_HIDDEN),
    NS_ERROR_FAILURE);
  return NS_OK;
}

void
nsDocumentViewer::CallChildren(CallChildFunc aFunc, void* aClosure)
{
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell)
  {
    int32_t i;
    int32_t n;
    docShell->GetChildCount(&n);
    for (i=0; i < n; i++)
    {
      nsCOMPtr<nsIDocShellTreeItem> child;
      docShell->GetChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      NS_ASSERTION(childAsShell, "null child in docshell");
      if (childAsShell)
      {
        nsCOMPtr<nsIContentViewer> childCV;
        childAsShell->GetContentViewer(getter_AddRefs(childCV));
        if (childCV)
        {
          (*aFunc)(childCV, aClosure);
        }
      }
    }
  }
}

static void
ChangeChildPaintingEnabled(nsIContentViewer* aChild, void* aClosure)
{
  bool* enablePainting = (bool*) aClosure;
  if (*enablePainting) {
    aChild->ResumePainting();
  } else {
    aChild->PausePainting();
  }
}

struct ZoomInfo
{
  float mZoom;
};

static void
SetChildTextZoom(nsIContentViewer* aChild, void* aClosure)
{
  struct ZoomInfo* ZoomInfo = (struct ZoomInfo*) aClosure;
  aChild->SetTextZoom(ZoomInfo->mZoom);
}

static void
SetChildMinFontSize(nsIContentViewer* aChild, void* aClosure)
{
  aChild->SetMinFontSize(NS_PTR_TO_INT32(aClosure));
}

static void
SetChildFullZoom(nsIContentViewer* aChild, void* aClosure)
{
  struct ZoomInfo* ZoomInfo = (struct ZoomInfo*) aClosure;
  aChild->SetFullZoom(ZoomInfo->mZoom);
}

static void
SetChildOverrideDPPX(nsIContentViewer* aChild, void* aClosure)
{
  struct ZoomInfo* ZoomInfo = (struct ZoomInfo*) aClosure;
  aChild->SetOverrideDPPX(ZoomInfo->mZoom);
}

static bool
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

  return true;
}

static bool
SetExtResourceMinFontSize(nsIDocument* aDocument, void* aClosure)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      ctxt->SetBaseMinFontSize(NS_PTR_TO_INT32(aClosure));
    }
  }

  return true;
}

static bool
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

  return true;
}

static bool
SetExtResourceOverrideDPPX(nsIDocument* aDocument, void* aClosure)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      struct ZoomInfo* ZoomInfo = static_cast<struct ZoomInfo*>(aClosure);
      ctxt->SetOverrideDPPX(ZoomInfo->mZoom);
    }
  }

  return true;
}

NS_IMETHODIMP
nsDocumentViewer::SetTextZoom(float aTextZoom)
{
  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  if (GetIsPrintPreview()) {
    return NS_OK;
  }

  mTextZoom = aTextZoom;

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

  nsContentUtils::DispatchChromeEvent(mDocument, static_cast<nsIDocument*>(mDocument),
                                      NS_LITERAL_STRING("TextZoomChange"),
                                      true, true);

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetTextZoom(float* aTextZoom)
{
  NS_ENSURE_ARG_POINTER(aTextZoom);
  nsPresContext* pc = GetPresContext();
  *aTextZoom = pc ? pc->TextZoom() : 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetMinFontSize(int32_t aMinFontSize)
{
  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  if (GetIsPrintPreview()) {
    return NS_OK;
  }

  mMinFontSize = aMinFontSize;

  // Set the min font on all children of mContainer (even if our min font didn't
  // change, our children's min font may be different, though it would be unusual).
  // Do this first, in case kids are auto-sizing and post reflow commands on
  // our presshell (which should be subsumed into our own style change reflow).
  CallChildren(SetChildMinFontSize, NS_INT32_TO_PTR(aMinFontSize));

  // Now change our own min font
  nsPresContext* pc = GetPresContext();
  if (pc && aMinFontSize != mPresContext->MinFontSize(nullptr)) {
    pc->SetBaseMinFontSize(aMinFontSize);
  }

  // And do the external resources
  mDocument->EnumerateExternalResources(SetExtResourceMinFontSize,
                                        NS_INT32_TO_PTR(aMinFontSize));

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetMinFontSize(int32_t* aMinFontSize)
{
  NS_ENSURE_ARG_POINTER(aMinFontSize);
  nsPresContext* pc = GetPresContext();
  *aMinFontSize = pc ? pc->BaseMinFontSize() : 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetFullZoom(float aFullZoom)
{
#ifdef NS_PRINT_PREVIEW
  if (GetIsPrintPreview()) {
    nsPresContext* pc = GetPresContext();
    NS_ENSURE_TRUE(pc, NS_OK);
    nsCOMPtr<nsIPresShell> shell = pc->GetPresShell();
    NS_ENSURE_TRUE(shell, NS_OK);

    if (!mPrintPreviewZoomed) {
      mOriginalPrintPreviewScale = pc->GetPrintPreviewScale();
      mPrintPreviewZoomed = true;
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
      rootFrame->InvalidateFrame();
    }
    return NS_OK;
  }
#endif

  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  bool fullZoomChange = (mPageZoom != aFullZoom);
  mPageZoom = aFullZoom;

  struct ZoomInfo ZoomInfo = { aFullZoom };
  CallChildren(SetChildFullZoom, &ZoomInfo);

  nsPresContext* pc = GetPresContext();
  if (pc) {
    pc->SetFullZoom(aFullZoom);
  }

  // And do the external resources
  mDocument->EnumerateExternalResources(SetExtResourceFullZoom, &ZoomInfo);

  // Dispatch FullZoomChange event only if fullzoom value really was been changed
  if (fullZoomChange) {
    nsContentUtils::DispatchChromeEvent(mDocument, static_cast<nsIDocument*>(mDocument),
                                        NS_LITERAL_STRING("FullZoomChange"),
                                        true, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetFullZoom(float* aFullZoom)
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

NS_IMETHODIMP
nsDocumentViewer::SetOverrideDPPX(float aDPPX)
{
  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  mOverrideDPPX = aDPPX;

  struct ZoomInfo ZoomInfo = { aDPPX };
  CallChildren(SetChildOverrideDPPX, &ZoomInfo);

  nsPresContext* pc = GetPresContext();
  if (pc) {
    pc->SetOverrideDPPX(aDPPX);
  }

  // And do the external resources
  mDocument->EnumerateExternalResources(SetExtResourceOverrideDPPX, &ZoomInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetOverrideDPPX(float* aDPPX)
{
  NS_ENSURE_ARG_POINTER(aDPPX);

  nsPresContext* pc = GetPresContext();
  *aDPPX = pc ? pc->GetOverrideDPPX() : mOverrideDPPX;
  return NS_OK;
}

static void
SetChildAuthorStyleDisabled(nsIContentViewer* aChild, void* aClosure)
{
  bool styleDisabled  = *static_cast<bool*>(aClosure);
  aChild->SetAuthorStyleDisabled(styleDisabled);
}


NS_IMETHODIMP
nsDocumentViewer::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  if (mPresShell) {
    mPresShell->SetAuthorStyleDisabled(aStyleDisabled);
  }
  CallChildren(SetChildAuthorStyleDisabled, &aStyleDisabled);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetAuthorStyleDisabled(bool* aStyleDisabled)
{
  if (mPresShell) {
    *aStyleDisabled = mPresShell->GetAuthorStyleDisabled();
  } else {
    *aStyleDisabled = false;
  }
  return NS_OK;
}

static bool
ExtResourceEmulateMedium(nsIDocument* aDocument, void* aClosure)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      const nsAString* mediaType = static_cast<nsAString*>(aClosure);
      ctxt->EmulateMedium(*mediaType);
    }
  }

  return true;
}

static void
ChildEmulateMedium(nsIContentViewer* aChild, void* aClosure)
{
  const nsAString* mediaType = static_cast<nsAString*>(aClosure);
  aChild->EmulateMedium(*mediaType);
}

NS_IMETHODIMP
nsDocumentViewer::EmulateMedium(const nsAString& aMediaType)
{
  if (mPresContext) {
    mPresContext->EmulateMedium(aMediaType);
  }
  CallChildren(ChildEmulateMedium, const_cast<nsAString*>(&aMediaType));

  if (mDocument) {
    mDocument->EnumerateExternalResources(ExtResourceEmulateMedium,
                                          const_cast<nsAString*>(&aMediaType));
  }

  return NS_OK;
}

static bool
ExtResourceStopEmulatingMedium(nsIDocument* aDocument, void* aClosure)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* ctxt = shell->GetPresContext();
    if (ctxt) {
      ctxt->StopEmulatingMedium();
    }
  }

  return true;
}

static void
ChildStopEmulatingMedium(nsIContentViewer* aChild, void* aClosure)
{
  aChild->StopEmulatingMedium();
}

NS_IMETHODIMP
nsDocumentViewer::StopEmulatingMedium()
{
  if (mPresContext) {
    mPresContext->StopEmulatingMedium();
  }
  CallChildren(ChildStopEmulatingMedium, nullptr);

  if (mDocument) {
    mDocument->EnumerateExternalResources(ExtResourceStopEmulatingMedium,
                                          nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetForceCharacterSet(nsACString& aForceCharacterSet)
{
  aForceCharacterSet = mForceCharacterSet;
  return NS_OK;
}

static void
SetChildForceCharacterSet(nsIContentViewer* aChild, void* aClosure)
{
  const nsACString* charset = static_cast<nsACString*>(aClosure);
  aChild->SetForceCharacterSet(*charset);
}

NS_IMETHODIMP
nsDocumentViewer::SetForceCharacterSet(const nsACString& aForceCharacterSet)
{
  // This method is scriptable, so add-ons could pass in something other
  // than a canonical name. However, in case where the input is a canonical
  // name, "replacement" doesn't survive label resolution. Additionally, the
  // empty string means no hint.
  nsAutoCString encoding;
  if (!aForceCharacterSet.IsEmpty()) {
    if (aForceCharacterSet.EqualsLiteral("replacement")) {
      encoding.AssignLiteral("replacement");
    } else if (!EncodingUtils::FindEncodingForLabel(aForceCharacterSet,
                                                    encoding)) {
      // Reject unknown labels
      return NS_ERROR_INVALID_ARG;
    }
  }
  mForceCharacterSet = encoding;
  // now set the force char set on all children of mContainer
  CallChildren(SetChildForceCharacterSet, (void*) &aForceCharacterSet);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetHintCharacterSet(nsACString& aHintCharacterSet)
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

NS_IMETHODIMP nsDocumentViewer::GetHintCharacterSetSource(int32_t *aHintCharacterSetSource)
{
  NS_ENSURE_ARG_POINTER(aHintCharacterSetSource);

  *aHintCharacterSetSource = mHintCharsetSource;
  return NS_OK;
}

static void
SetChildHintCharacterSetSource(nsIContentViewer* aChild, void* aClosure)
{
  aChild->SetHintCharacterSetSource(NS_PTR_TO_INT32(aClosure));
}

NS_IMETHODIMP
nsDocumentViewer::SetHintCharacterSetSource(int32_t aHintCharacterSetSource)
{
  mHintCharsetSource = aHintCharacterSetSource;
  // now set the hint char set source on all children of mContainer
  CallChildren(SetChildHintCharacterSetSource,
                      NS_INT32_TO_PTR(aHintCharacterSetSource));
  return NS_OK;
}

static void
SetChildHintCharacterSet(nsIContentViewer* aChild, void* aClosure)
{
  const nsACString* charset = static_cast<nsACString*>(aClosure);
  aChild->SetHintCharacterSet(*charset);
}

NS_IMETHODIMP
nsDocumentViewer::SetHintCharacterSet(const nsACString& aHintCharacterSet)
{
  // This method is scriptable, so add-ons could pass in something other
  // than a canonical name. However, in case where the input is a canonical
  // name, "replacement" doesn't survive label resolution. Additionally, the
  // empty string means no hint.
  nsAutoCString encoding;
  if (!aHintCharacterSet.IsEmpty()) {
    if (aHintCharacterSet.EqualsLiteral("replacement")) {
      encoding.AssignLiteral("replacement");
    } else if (!EncodingUtils::FindEncodingForLabel(aHintCharacterSet,
                                                    encoding)) {
      // Reject unknown labels
      return NS_ERROR_INVALID_ARG;
    }
  }
  mHintCharset = encoding;
  // now set the hint char set on all children of mContainer
  CallChildren(SetChildHintCharacterSet, (void*) &aHintCharacterSet);
  return NS_OK;
}

static void
AppendChildSubtree(nsIContentViewer* aChild, void* aClosure)
{
  nsTArray<nsCOMPtr<nsIContentViewer> >& array =
    *static_cast<nsTArray<nsCOMPtr<nsIContentViewer> >*>(aClosure);
  aChild->AppendSubtree(array);
}

NS_IMETHODIMP nsDocumentViewer::AppendSubtree(nsTArray<nsCOMPtr<nsIContentViewer> >& aArray)
{
  aArray.AppendElement(this);
  CallChildren(AppendChildSubtree, &aArray);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::PausePainting()
{
  bool enablePaint = false;
  CallChildren(ChangeChildPaintingEnabled, &enablePaint);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    presShell->PausePainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::ResumePainting()
{
  bool enablePaint = true;
  CallChildren(ChangeChildPaintingEnabled, &enablePaint);

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    presShell->ResumePainting();
  }

  return NS_OK;
}

nsresult
nsDocumentViewer::GetContentSizeInternal(int32_t* aWidth, int32_t* aHeight,
                                         nscoord aMaxWidth, nscoord aMaxHeight)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

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
    nsRenderingContext rcx(presShell->CreateReferenceRenderingContext());
    prefWidth = root->GetPrefISize(&rcx);
  }
  if (prefWidth > aMaxWidth) {
    prefWidth = aMaxWidth;
  }

  nsresult rv = presShell->ResizeReflow(prefWidth, NS_UNCONSTRAINEDSIZE);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsPresContext> presContext;
  GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  // so how big is it?
  nsRect shellArea = presContext->GetVisibleArea();
  if (shellArea.height > aMaxHeight) {
    // Reflow to max height if we would up too tall.
    rv = presShell->ResizeReflow(prefWidth, aMaxHeight);
    NS_ENSURE_SUCCESS(rv, rv);

    shellArea = presContext->GetVisibleArea();
  }

  // Protect against bogus returns here
  NS_ENSURE_TRUE(shellArea.width != NS_UNCONSTRAINEDSIZE &&
                 shellArea.height != NS_UNCONSTRAINEDSIZE,
                 NS_ERROR_FAILURE);

  *aWidth = presContext->AppUnitsToDevPixels(shellArea.width);
  *aHeight = presContext->AppUnitsToDevPixels(shellArea.height);

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetContentSize(int32_t* aWidth, int32_t* aHeight)
{
  // Skip doing this on docshell-less documents for now
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(mContainer);
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top frame.  Doesn't work from
  // sub-frames.
  NS_ENSURE_TRUE(!docShellParent, NS_ERROR_FAILURE);

  return GetContentSizeInternal(aWidth, aHeight, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

NS_IMETHODIMP
nsDocumentViewer::GetContentSizeConstrained(int32_t aMaxWidth, int32_t aMaxHeight,
                                            int32_t* aWidth, int32_t* aHeight)
{
  RefPtr<nsPresContext> presContext;
  GetPresContext(getter_AddRefs(presContext));
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nscoord maxWidth = NS_UNCONSTRAINEDSIZE;
  nscoord maxHeight = NS_UNCONSTRAINEDSIZE;
  if (aMaxWidth > 0) {
    maxWidth = presContext->DevPixelsToAppUnits(aMaxWidth);
  }
  if (aMaxHeight > 0) {
    maxHeight = presContext->DevPixelsToAppUnits(aMaxHeight);
  }

  return GetContentSizeInternal(aWidth, aHeight, maxWidth, maxHeight);
}


NS_IMPL_ISUPPORTS(nsDocViewerSelectionListener, nsISelectionListener)

nsresult nsDocViewerSelectionListener::Init(nsDocumentViewer *aDocViewer)
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
nsDocumentViewer::GetPopupNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  *aNode = nullptr;

  // get the document
  nsIDocument* document = GetDocument();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  // get the private dom window
  nsCOMPtr<nsPIDOMWindowOuter> window(document->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);
  if (window) {
    nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
    NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

    // get the popup node
    nsCOMPtr<nsIDOMNode> node = root->GetPopupNode();
#ifdef MOZ_XUL
    if (!node) {
      nsPIDOMWindowOuter* rootWindow = root->GetWindow();
      if (rootWindow) {
        nsCOMPtr<nsIDocument> rootDoc = rootWindow->GetExtantDoc();
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
nsDocumentViewer::GetPopupLinkNode(nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nullptr;

  // find popup node
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  // find out if we have a link in our ancestry
  while (node) {

    nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    if (content) {
      nsCOMPtr<nsIURI> hrefURI = content->GetHrefURI();
      if (hrefURI) {
        *aNode = node;
        NS_IF_ADDREF(*aNode); // addref
        return NS_OK;
      }
    }

    // get our parent and keep trying...
    nsCOMPtr<nsIDOMNode> parentNode;
    node->GetParentNode(getter_AddRefs(parentNode));
    node = parentNode;
  }

  // if we have no node, fail
  return NS_ERROR_FAILURE;
}

// GetPopupLinkNode: return popup image node or fail
nsresult
nsDocumentViewer::GetPopupImageNode(nsIImageLoadingContent** aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  // you get null unless i say so
  *aNode = nullptr;

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

NS_IMETHODIMP nsDocumentViewer::GetInLink(bool* aInLink)
{
#ifdef DEBUG_dr
  printf("dr :: nsDocumentViewer::GetInLink\n");
#endif

  NS_ENSURE_ARG_POINTER(aInLink);

  // we're not in a link unless i say so
  *aInLink = false;

  // get the popup link
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = GetPopupLinkNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // if we made it here, we're in a link
  *aInLink = true;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetInImage(bool* aInImage)
{
#ifdef DEBUG_dr
  printf("dr :: nsDocumentViewer::GetInImage\n");
#endif

  NS_ENSURE_ARG_POINTER(aInImage);

  // we're not in an image unless i say so
  *aInImage = false;

  // get the popup image
  nsCOMPtr<nsIImageLoadingContent> node;
  nsresult rv = GetPopupImageNode(getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  // Make sure there is a URI assigned. This allows <input type="image"> to
  // be an image but rejects other <input> types. This matches what
  // nsContextMenu.js does.
  nsCOMPtr<nsIURI> uri;
  node->GetCurrentURI(getter_AddRefs(uri));
  if (uri) {
    // if we made it here, we're in an image
    *aInImage = true;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocViewerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *, int16_t aReason)
{
  NS_ASSERTION(mDocViewer, "Should have doc viewer!");

  // get the selection state
  RefPtr<mozilla::dom::Selection> selection = mDocViewer->GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  nsIDocument* theDoc = mDocViewer->GetDocument();
  if (!theDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIDOMWindowOuter> domWindow = theDoc->GetWindow();
  if (!domWindow) return NS_ERROR_FAILURE;

  bool selectionCollapsed;
  selection->GetIsCollapsed(&selectionCollapsed);
  // We only call UpdateCommands when the selection changes from collapsed to
  // non-collapsed or vice versa, however we skip the initializing collapse. We
  // might need another update string for simple selection changes, but that
  // would be expenseive.
  if (mSelectionWasCollapsed != selectionCollapsed)
  {
    domWindow->UpdateCommands(NS_LITERAL_STRING("select"), selection, aReason);
    mSelectionWasCollapsed = selectionCollapsed;
  }

  return NS_OK;
}

//nsDocViewerFocusListener
NS_IMPL_ISUPPORTS(nsDocViewerFocusListener,
                  nsIDOMEventListener)

nsDocViewerFocusListener::nsDocViewerFocusListener()
:mDocViewer(nullptr)
{
}

nsDocViewerFocusListener::~nsDocViewerFocusListener(){}

nsresult
nsDocViewerFocusListener::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ENSURE_STATE(mDocViewer);

  nsCOMPtr<nsIPresShell> shell;
  mDocViewer->GetPresShell(getter_AddRefs(shell));
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(shell);
  int16_t selectionStatus;
  selCon->GetDisplaySelection(&selectionStatus);

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("focus")) {
    // If selection was disabled, re-enable it.
    if(selectionStatus == nsISelectionController::SELECTION_DISABLED ||
       selectionStatus == nsISelectionController::SELECTION_HIDDEN) {
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
    }
  } else {
    MOZ_ASSERT(eventType.EqualsLiteral("blur"), "Unexpected event type");
    // If selection was on, disable it.
    if(selectionStatus == nsISelectionController::SELECTION_ON ||
       selectionStatus == nsISelectionController::SELECTION_ATTENTION) {
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
      selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
    }
  }

  return NS_OK;
}

nsresult
nsDocViewerFocusListener::Init(nsDocumentViewer *aDocViewer)
{
  mDocViewer = aDocViewer;
  return NS_OK;
}

/** ---------------------------------------------------
 *  From nsIWebBrowserPrint
 */

#ifdef NS_PRINTING

NS_IMETHODIMP
nsDocumentViewer::Print(nsIPrintSettings*       aPrintSettings,
                          nsIWebProgressListener* aWebProgressListener)
{
  // Printing XUL documents is not supported.
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    return NS_ERROR_FAILURE;
  }

  if (!mContainer) {
    PR_PL(("Container was destroyed yet we are still trying to use it!"));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  NS_ENSURE_STATE(docShell);

  // Check to see if this document is still busy
  // If it is busy and we aren't already "queued" up to print then
  // Indicate there is a print pending and cache the args for later
  uint32_t busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  if ((NS_FAILED(docShell->GetBusyFlags(&busyFlags)) ||
       (busyFlags != nsIDocShell::BUSY_FLAGS_NONE && busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING)) && 
      !mPrintDocIsFullyLoaded) {
    if (!mPrintIsPending) {
      mCachedPrintSettings           = aPrintSettings;
      mCachedPrintWebProgressListner = aWebProgressListener;
      mPrintIsPending                = true;
    }
    PR_PL(("Printing Stopped - document is still busy!"));
    return NS_ERROR_GFX_PRINTER_DOC_IS_BUSY;
  }

  if (!mDocument || !mDeviceContext) {
    PR_PL(("Can't Print without a document and a device context"));
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

    if (mPrintEngine) {
      mPrintEngine->FirePrintingErrorEvent(rv);
    }

    return rv;
  }

  // Dispatch 'beforeprint' event and ensure 'afterprint' will be dispatched:
  nsAutoPtr<AutoPrintEventDispatcher> autoBeforeAndAfterPrint(
    new AutoPrintEventDispatcher(mDocument));
  NS_ENSURE_STATE(!GetIsPrinting());
  // If we are hosting a full-page plugin, tell it to print
  // first. It shows its own native print UI.
  nsCOMPtr<nsIPluginDocument> pDoc(do_QueryInterface(mDocument));
  if (pDoc)
    return pDoc->Print();

  if (!mPrintEngine) {
    NS_ENSURE_STATE(mDeviceContext);
    mPrintEngine = new nsPrintEngine();

    rv = mPrintEngine->Initialize(this, mContainer, mDocument, 
                                  float(mDeviceContext->AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom,
#ifdef DEBUG
                                  mDebugFile
#else
                                  nullptr
#endif
                                  );
    if (NS_FAILED(rv)) {
      mPrintEngine->Destroy();
      mPrintEngine = nullptr;
      return rv;
    }
  }
  if (mPrintEngine->HasPrintCallbackCanvas()) {
    // Postpone the 'afterprint' event until after the mozPrintCallback
    // callbacks have been called:
    mAutoBeforeAndAfterPrint = autoBeforeAndAfterPrint;
  }
  dom::Element* root = mDocument->GetRootElement();
  if (root && root->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdisallowselectionprint)) {
    mPrintEngine->SetDisallowSelectionPrint(true);
  }
  rv = mPrintEngine->Print(aPrintSettings, aWebProgressListener);
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
}

NS_IMETHODIMP
nsDocumentViewer::PrintPreview(nsIPrintSettings* aPrintSettings, 
                               mozIDOMWindowProxy* aChildDOMWin, 
                               nsIWebProgressListener* aWebProgressListener)
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  NS_WARNING_ASSERTION(
    IsInitializedForPrintPreview(),
    "Using docshell.printPreview is the preferred way for print previewing!");

  NS_ENSURE_ARG_POINTER(aChildDOMWin);
  nsresult rv = NS_OK;

  if (GetIsPrinting()) {
    nsPrintEngine::CloseProgressDialog(aWebProgressListener);
    return NS_ERROR_FAILURE;
  }

  // Printing XUL documents is not supported.
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    nsPrintEngine::CloseProgressDialog(aWebProgressListener);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (!docShell || !mDeviceContext) {
    PR_PL(("Can't Print Preview without device context and docshell"));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aChildDOMWin);
  MOZ_ASSERT(window);
  nsCOMPtr<nsIDocument> doc = window->GetDoc();
  NS_ENSURE_STATE(doc);

  // Dispatch 'beforeprint' event and ensure 'afterprint' will be dispatched:
  nsAutoPtr<AutoPrintEventDispatcher> autoBeforeAndAfterPrint(
    new AutoPrintEventDispatcher(doc));
  NS_ENSURE_STATE(!GetIsPrinting());
  // beforeprint event may have caused ContentViewer to be shutdown.
  NS_ENSURE_STATE(mContainer);
  NS_ENSURE_STATE(mDeviceContext);
  if (!mPrintEngine) {
    mPrintEngine = new nsPrintEngine();

    rv = mPrintEngine->Initialize(this, mContainer, doc,
                                  float(mDeviceContext->AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom,
#ifdef DEBUG
                                  mDebugFile
#else
                                  nullptr
#endif
                                  );
    if (NS_FAILED(rv)) {
      mPrintEngine->Destroy();
      mPrintEngine = nullptr;
      return rv;
    }
  }
  if (mPrintEngine->HasPrintCallbackCanvas()) {
    // Postpone the 'afterprint' event until after the mozPrintCallback
    // callbacks have been called:
    mAutoBeforeAndAfterPrint = autoBeforeAndAfterPrint;
  }
  dom::Element* root = doc->GetRootElement();
  if (root && root->HasAttr(kNameSpaceID_None, nsGkAtoms::mozdisallowselectionprint)) {
    PR_PL(("PrintPreview: found mozdisallowselectionprint"));
    mPrintEngine->SetDisallowSelectionPrint(true);
  }
  rv = mPrintEngine->PrintPreview(aPrintSettings, aChildDOMWin, aWebProgressListener);
  mPrintPreviewZoomed = false;
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
nsDocumentViewer::PrintPreviewNavigate(int16_t aType, int32_t aPageNum)
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
  nsIFrame* seqFrame  = nullptr;
  int32_t   pageCount = 0;
  if (NS_FAILED(mPrintEngine->GetSeqFrameAndCountPages(seqFrame, pageCount))) {
    return NS_ERROR_FAILURE;
  }

  // Figure where we are currently scrolled to
  nsPoint pt = sf->GetScrollPosition();

  int32_t    pageNum = 1;
  nsIFrame * fndPageFrame  = nullptr;
  nsIFrame * currentPage   = nullptr;

  // If it is "End" then just do a "goto" to the last page
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_END) {
    aType    = nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM;
    aPageNum = pageCount;
  }

  // Now, locate the current page we are on and
  // and the page of the page number
  for (nsIFrame* pageFrame : seqFrame->PrincipalChildList()) {
    nsRect pageRect = pageFrame->GetRect();
    if (pageRect.Contains(pageRect.x, pt.y)) {
      currentPage = pageFrame;
    }
    if (pageNum == aPageNum) {
      fndPageFrame = pageFrame;
      break;
    }
    pageNum++;
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
    nscoord newYPosn =
      nscoord(mPrintEngine->GetPrintPreviewScale() * fndPageFrame->GetPosition().y);
    sf->ScrollTo(nsPoint(pt.x, newYPosn), nsIScrollableFrame::INSTANT);
  }
  return NS_OK;

}

NS_IMETHODIMP
nsDocumentViewer::GetGlobalPrintSettings(nsIPrintSettings * *aGlobalPrintSettings)
{
  return nsPrintEngine::GetGlobalPrintSettings(aGlobalPrintSettings);
}

// XXX This always returns false for subdocuments
NS_IMETHODIMP
nsDocumentViewer::GetDoingPrint(bool *aDoingPrint)
{
  NS_ENSURE_ARG_POINTER(aDoingPrint);
  
  *aDoingPrint = false;
  if (mPrintEngine) {
    // XXX shouldn't this be GetDoingPrint() ?
    return mPrintEngine->GetDoingPrintPreview(aDoingPrint);
  } 
  return NS_OK;
}

// XXX This always returns false for subdocuments
NS_IMETHODIMP
nsDocumentViewer::GetDoingPrintPreview(bool *aDoingPrintPreview)
{
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);

  *aDoingPrintPreview = false;
  if (mPrintEngine) {
    return mPrintEngine->GetDoingPrintPreview(aDoingPrintPreview);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetCurrentPrintSettings(nsIPrintSettings * *aCurrentPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  *aCurrentPrintSettings = nullptr;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetCurrentPrintSettings(aCurrentPrintSettings);
}


NS_IMETHODIMP 
nsDocumentViewer::GetCurrentChildDOMWindow(mozIDOMWindowProxy** aCurrentChildDOMWindow)
{
  NS_ENSURE_ARG_POINTER(aCurrentChildDOMWindow);
  *aCurrentChildDOMWindow = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentViewer::Cancel()
{
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);
  return mPrintEngine->Cancelled();
}

NS_IMETHODIMP
nsDocumentViewer::ExitPrintPreview()
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
nsDocumentViewer::EnumerateDocumentNames(uint32_t* aCount,
                                           char16_t*** aResult)
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

NS_IMETHODIMP 
nsDocumentViewer::GetIsFramesetFrameSelected(bool *aIsFramesetFrameSelected)
{
#ifdef NS_PRINTING
  *aIsFramesetFrameSelected = false;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsFramesetFrameSelected(aIsFramesetFrameSelected);
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
nsDocumentViewer::GetPrintPreviewNumPages(int32_t *aPrintPreviewNumPages)
{
#ifdef NS_PRINTING
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetPrintPreviewNumPages(aPrintPreviewNumPages);
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
nsDocumentViewer::GetIsFramesetDocument(bool *aIsFramesetDocument)
{
#ifdef NS_PRINTING
  *aIsFramesetDocument = false;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsFramesetDocument(aIsFramesetDocument);
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP 
nsDocumentViewer::GetIsIFrameSelected(bool *aIsIFrameSelected)
{
#ifdef NS_PRINTING
  *aIsIFrameSelected = false;
  NS_ENSURE_TRUE(mPrintEngine, NS_ERROR_FAILURE);

  return mPrintEngine->GetIsIFrameSelected(aIsIFrameSelected);
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP 
nsDocumentViewer::GetIsRangeSelection(bool *aIsRangeSelection)
{
#ifdef NS_PRINTING
  *aIsRangeSelection = false;
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
nsDocumentViewer::SetIsPrintingInDocShellTree(nsIDocShellTreeItem* aParentNode, 
                                                bool                 aIsPrintingOrPP, 
                                                bool                 aStartAtTop)
{
  nsCOMPtr<nsIDocShellTreeItem> parentItem(do_QueryInterface(aParentNode));

  // find top of "same parent" tree
  if (aStartAtTop) {
    if (aIsPrintingOrPP) {
      while (parentItem) {
        nsCOMPtr<nsIDocShellTreeItem> parent;
        parentItem->GetSameTypeParent(getter_AddRefs(parent));
        if (!parent) {
          break;
        }
        parentItem = do_QueryInterface(parent);
      }
      mTopContainerWhilePrinting = do_GetWeakReference(parentItem);
    } else {
      parentItem = do_QueryReferent(mTopContainerWhilePrinting);
    }
  }

  // Check to see if the DocShell's ContentViewer is printing/PP
  nsCOMPtr<nsIContentViewerContainer> viewerContainer(do_QueryInterface(parentItem));
  if (viewerContainer) {
    viewerContainer->SetIsPrinting(aIsPrintingOrPP);
  }

  if (!aParentNode) {
    return;
  }

  // Traverse children to see if any of them are printing.
  int32_t n;
  aParentNode->GetChildCount(&n);
  for (int32_t i=0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aParentNode->GetChildAt(i, getter_AddRefs(child));
    NS_ASSERTION(child, "child isn't nsIDocShell");
    if (child) {
      SetIsPrintingInDocShellTree(child, aIsPrintingOrPP, false);
    }
  }

}
#endif // NS_PRINTING

bool
nsDocumentViewer::ShouldAttachToTopLevel()
{
  if (!mParentWidget)
    return false;

  nsCOMPtr<nsIDocShellTreeItem> containerItem(mContainer);
  if (!containerItem)
    return false;

  // We always attach when using puppet widgets
  if (nsIWidget::UsePuppetWidgets())
    return true;

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  // On windows, in the parent process we also attach, but just to
  // chrome items
  nsWindowType winType = mParentWidget->WindowType();
  if ((winType == eWindowType_toplevel ||
       winType == eWindowType_dialog ||
       winType == eWindowType_invisible) &&
      containerItem->ItemType() == nsIDocShellTreeItem::typeChrome) {
    return true;
  }
#endif

  return false;
}

//------------------------------------------------------------
// XXX this always returns false for subdocuments
bool
nsDocumentViewer::GetIsPrinting()
{
#ifdef NS_PRINTING
  if (mPrintEngine) {
    return mPrintEngine->GetIsPrinting();
  }
#endif
  return false; 
}

//------------------------------------------------------------
// Notification from the PrintEngine of the current Printing status
void
nsDocumentViewer::SetIsPrinting(bool aIsPrinting)
{
#ifdef NS_PRINTING
  // Set all the docShells in the docshell tree to be printing.
  // that way if anyone of them tries to "navigate" it can't
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell || !aIsPrinting) {
    SetIsPrintingInDocShellTree(docShell, aIsPrinting, true);
  } else {
    NS_WARNING("Did you close a window before printing?");
  }

  if (!aIsPrinting) {
    // Dispatch the 'afterprint' event now, if pending:
    mAutoBeforeAndAfterPrint = nullptr;
  }
#endif
}

//------------------------------------------------------------
// The PrintEngine holds the current value
// this called from inside the DocViewer.
// XXX it always returns false for subdocuments
bool
nsDocumentViewer::GetIsPrintPreview()
{
#ifdef NS_PRINTING
  if (mPrintEngine) {
    return mPrintEngine->GetIsPrintPreview();
  }
#endif
  return false; 
}

//------------------------------------------------------------
// Notification from the PrintEngine of the current PP status
void
nsDocumentViewer::SetIsPrintPreview(bool aIsPrintPreview)
{
#ifdef NS_PRINTING
  // Set all the docShells in the docshell tree to be printing.
  // that way if anyone of them tries to "navigate" it can't
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (docShell || !aIsPrintPreview) {
    SetIsPrintingInDocShellTree(docShell, aIsPrintPreview, true);
  }
  if (!aIsPrintPreview) {
    // Dispatch the 'afterprint' event now, if pending:
    mAutoBeforeAndAfterPrint = nullptr;
  }
#endif
  if (!aIsPrintPreview) {
    if (mPresShell) {
      DestroyPresShell();
    }
    mWindow = nullptr;
    mViewManager = nullptr;
    mPresContext = nullptr;
    mPresShell = nullptr;
  }
}

//----------------------------------------------------------------------------------
// nsIDocumentViewerPrint IFace
//----------------------------------------------------------------------------------

//------------------------------------------------------------
void
nsDocumentViewer::IncrementDestroyRefCount()
{
  ++mDestroyRefCount;
}

//------------------------------------------------------------

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
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
  bool hasMoreDocShells;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
         && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    nsCOMPtr<nsPIDOMWindowOuter> win = do_GetInterface(currentContainer);
    if (win)
      fm->ClearFocus(win);
  }
}
#endif // NS_PRINTING && NS_PRINT_PREVIEW

void
nsDocumentViewer::ReturnToGalleyPresentation()
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (!GetIsPrintPreview()) {
    NS_ERROR("Wow, we should never get here!");
    return;
  }

  SetIsPrintPreview(false);

  mPrintEngine->TurnScriptingOn(true);
  mPrintEngine->Destroy();
  mPrintEngine = nullptr;

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  ResetFocusState(docShell);

  SetTextZoom(mTextZoom);
  SetFullZoom(mPageZoom);
  SetOverrideDPPX(mOverrideDPPX);
  SetMinFontSize(mMinFontSize);
  Show();

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
nsDocumentViewer::OnDonePrinting() 
{
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  if (mPrintEngine) {
    RefPtr<nsPrintEngine> pe = mPrintEngine;
    if (GetIsPrintPreview()) {
      pe->DestroyPrintingData();
    } else {
      mPrintEngine = nullptr;
      pe->Destroy();
    }

    // We are done printing, now cleanup 
    if (mDeferredWindowClose) {
      mDeferredWindowClose = false;
      if (mContainer) {
        if (nsCOMPtr<nsPIDOMWindowOuter> win = do_QueryInterface(mContainer->GetWindow())) {
          win->Close();
        }
      }
    } else if (mClosingWhilePrinting) {
      if (mDocument) {
        mDocument->Destroy();
        mDocument = nullptr;
      }
      mClosingWhilePrinting = false;
    }
  }
#endif // NS_PRINTING && NS_PRINT_PREVIEW
}

NS_IMETHODIMP nsDocumentViewer::SetPageMode(bool aPageMode, nsIPrintSettings* aPrintSettings)
{
  // XXX Page mode is only partially working; it's currently used for
  // reftests that require a paginated context
  mIsPageMode = aPageMode;

  if (mPresShell) {
    DestroyPresShell();
  }

  if (mPresContext) {
    DestroyPresContext();
  }

  mViewManager  = nullptr;
  mWindow       = nullptr;

  NS_ENSURE_STATE(mDocument);
  if (aPageMode)
  {    
    mPresContext = CreatePresContext(mDocument,
        nsPresContext::eContext_PageLayout, FindContainerView());
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);
    mPresContext->SetPaginatedScrolling(true);
    mPresContext->SetPrintSettings(aPrintSettings);
    nsresult rv = mPresContext->Init(mDeviceContext);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(InitInternal(mParentWidget, nullptr, mBounds, true, false),
                    NS_ERROR_FAILURE);

  Show();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetHistoryEntry(nsISHEntry **aHistoryEntry)
{
  NS_IF_ADDREF(*aHistoryEntry = mSHEntry);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsTabModalPromptAllowed(bool *aAllowed)
{
  *aAllowed = !mHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsHidden(bool *aHidden)
{
  *aHidden = mHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetIsHidden(bool aHidden)
{
  mHidden = aHidden;
  return NS_OK;
}

void
nsDocumentViewer::DestroyPresShell()
{
  // Break circular reference (or something)
  mPresShell->EndObservingDocument();

  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (selection && mSelectionListener)
    selection->RemoveSelectionListener(mSelectionListener);

  nsAutoScriptBlocker scriptBlocker;
  mPresShell->Destroy();
  mPresShell = nullptr;
}

void
nsDocumentViewer::DestroyPresContext()
{
  mPresContext->Detach();
  mPresContext = nullptr;
}

bool
nsDocumentViewer::IsInitializedForPrintPreview()
{
  return mInitializedForPrintPreview;
}

void
nsDocumentViewer::InitializeForPrintPreview()
{
  mInitializedForPrintPreview = true;
}

void
nsDocumentViewer::SetPrintPreviewPresentation(nsViewManager* aViewManager,
                                              nsPresContext* aPresContext,
                                              nsIPresShell* aPresShell)
{
  if (mPresShell) {
    DestroyPresShell();
  }

  mWindow = nullptr;
  mViewManager = aViewManager;
  mPresContext = aPresContext;
  mPresShell = aPresShell;

  if (ShouldAttachToTopLevel()) {
    DetachFromTopLevelWidget();
    nsView* rootView = mViewManager->GetRootView();
    rootView->AttachToTopLevelWidget(mParentWidget);
    mAttachedToParent = true;
  }
}

// Fires the "document-shown" event so that interested parties are aware of it.
NS_IMETHODIMP
nsDocumentShownDispatcher::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mDocument, "document-shown", nullptr);
  }
  return NS_OK;
}


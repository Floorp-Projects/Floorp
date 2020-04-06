/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* container for a document and its presentation */

#include "gfxContext.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoStyleSet.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsFrameSelection.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewerPrint.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BeforeUnloadEvent.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DocGroup.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIWritablePropertyBag2.h"
#include "nsSubDocumentFrame.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"

#include "nsISelectionListener.h"
#include "mozilla/dom/Selection.h"
#include "nsContentUtils.h"
#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessible.h"
#endif
#include "mozilla/BasicEvents.h"
#include "mozilla/Encoding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

#include "nsViewManager.h"
#include "nsView.h"

#include "nsPageSequenceFrame.h"
#include "nsNetUtil.h"
#include "nsIContentViewerEdit.h"
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
#ifdef MOZ_XUL
#  include "nsXULPopupManager.h"
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
#include "nsILoadContext.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsIPrompt.h"
#include "imgIContainer.h"  // image animation mode constants

#include "nsSandboxFlags.h"

#include "mozilla/DocLoadingTimelineMarker.h"

//--------------------------
// Printing Include
//---------------------------
#ifdef NS_PRINTING

#  include "nsIWebBrowserPrint.h"

#  include "nsPrintJob.h"

// Print Options
#  include "nsIPrintSettings.h"
#  include "nsIPrintSettingsService.h"
#  include "nsISimpleEnumerator.h"

#  include "nsIPluginDocument.h"

#endif  // NS_PRINTING

// focus
#include "nsIDOMEventListener.h"
#include "nsISelectionController.h"

#include "mozilla/EventDispatcher.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "mozilla/dom/XMLHttpRequestMainThread.h"

// paint forcing
#include <stdio.h>
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/WindowGlobalChild.h"

using namespace mozilla;
using namespace mozilla::dom;

//-----------------------------------------------------
// LOGGING
#include "LayoutLogging.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gPageCacheLog;

#ifdef NS_PRINTING
static mozilla::LazyLogModule gPrintingLog("printing");

#  define PR_PL(_p1) MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);
#endif  // NS_PRINTING

#define PRT_YESNO(_p) ((_p) ? "YES" : "NO")
//-----------------------------------------------------

class nsDocumentViewer;
namespace mozilla {
class AutoPrintEventDispatcher;
}

// a small delegate class used to avoid circular references

class nsDocViewerSelectionListener final : public nsISelectionListener {
 public:
  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsISelectionListerner interface
  NS_DECL_NSISELECTIONLISTENER

  explicit nsDocViewerSelectionListener(nsDocumentViewer* aDocViewer)
      : mDocViewer(aDocViewer), mSelectionWasCollapsed(true) {}

  void Disconnect() { mDocViewer = nullptr; }

 protected:
  virtual ~nsDocViewerSelectionListener() = default;

  nsDocumentViewer* mDocViewer;
  bool mSelectionWasCollapsed;
};

/** editor Implementation of the FocusListener interface */
class nsDocViewerFocusListener final : public nsIDOMEventListener {
 public:
  explicit nsDocViewerFocusListener(nsDocumentViewer* aDocViewer)
      : mDocViewer(aDocViewer) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Disconnect() { mDocViewer = nullptr; }

 protected:
  virtual ~nsDocViewerFocusListener() = default;

  nsDocumentViewer* mDocViewer;
};

namespace viewer_detail {

/**
 * Mutation observer for use until we hand ourselves over to our SHEntry.
 */
class BFCachePreventionObserver final : public nsStubMutationObserver {
 public:
  explicit BFCachePreventionObserver(Document* aDocument)
      : mDocument(aDocument) {}

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  // Stop observing the document.
  void Disconnect();

 private:
  ~BFCachePreventionObserver() = default;

  // Helper for the work that needs to happen when mutations happen.
  void MutationHappened();

  Document* mDocument;  // Weak; we get notified if it dies
};

NS_IMPL_ISUPPORTS(BFCachePreventionObserver, nsIMutationObserver)

void BFCachePreventionObserver::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo&) {
  MutationHappened();
}

void BFCachePreventionObserver::AttributeChanged(Element* aElement,
                                                 int32_t aNameSpaceID,
                                                 nsAtom* aAttribute,
                                                 int32_t aModType,
                                                 const nsAttrValue* aOldValue) {
  MutationHappened();
}

void BFCachePreventionObserver::ContentAppended(nsIContent* aFirstNewContent) {
  MutationHappened();
}

void BFCachePreventionObserver::ContentInserted(nsIContent* aChild) {
  MutationHappened();
}

void BFCachePreventionObserver::ContentRemoved(nsIContent* aChild,
                                               nsIContent* aPreviousSibling) {
  MutationHappened();
}

void BFCachePreventionObserver::NodeWillBeDestroyed(const nsINode* aNode) {
  mDocument = nullptr;
}

void BFCachePreventionObserver::Disconnect() {
  if (mDocument) {
    mDocument->RemoveMutationObserver(this);
    // It will no longer tell us when it goes away, so make sure we're
    // not holding a dangling ref.
    mDocument = nullptr;
  }
}

void BFCachePreventionObserver::MutationHappened() {
  MOZ_ASSERT(
      mDocument,
      "How can we not have a document but be getting notified for mutations?");
  mDocument->DisallowBFCaching();
  Disconnect();
}

}  // namespace viewer_detail

using viewer_detail::BFCachePreventionObserver;

//-------------------------------------------------------------
class nsDocumentViewer final : public nsIContentViewer,
                               public nsIContentViewerEdit,
                               public nsIDocumentViewerPrint
#ifdef NS_PRINTING
    ,
                               public nsIWebBrowserPrint
#endif

{
  friend class nsDocViewerSelectionListener;
  friend class nsPagePrintTimer;
  friend class nsPrintJob;

 public:
  nsDocumentViewer();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIContentViewer interface...
  NS_DECL_NSICONTENTVIEWER

  // nsIContentViewerEdit
  NS_DECL_NSICONTENTVIEWEREDIT

#ifdef NS_PRINTING
  // nsIWebBrowserPrint
  NS_DECL_NSIWEBBROWSERPRINT
#endif

  using CallChildFunc = FunctionRef<void(nsDocumentViewer*)>;
  void CallChildren(CallChildFunc aFunc);

  using PresContextFunc = FunctionRef<void(nsPresContext*)>;
  /**
   * Calls a `CallChildFunc` on all children, a `PresContextFunc`
   * on all external documents' pres contexts  of our document, and then
   * finally on _this_ pres context, in that order.
   *
   * The children function is expected to call this function reentrantly, and
   * thus the `PresContextFunc` won't be called for the children's pres context
   * directly here.
   *
   * FIXME(emilio): Better name for this appreciated.
   */
  void PropagateToPresContextsHelper(CallChildFunc, PresContextFunc);

  // nsIDocumentViewerPrint Printing Methods
  NS_DECL_NSIDOCUMENTVIEWERPRINT

  void EmulateMediumInternal(nsAtom*);

  using ColorSchemeOverride = Maybe<StylePrefersColorScheme>;
  void EmulatePrefersColorSchemeInternal(const ColorSchemeOverride&);

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
  nsresult InitInternal(nsIWidget* aParentWidget, nsISupports* aState,
                        mozilla::dom::WindowGlobalChild* aActor,
                        const nsIntRect& aBounds, bool aDoCreation,
                        bool aNeedMakeCX = true,
                        bool aForceSetNewDocument = true);
  /**
   * @param aDoInitialReflow set to true if you want to kick off the initial
   * reflow
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult InitPresentationStuff(bool aDoInitialReflow);

  already_AddRefed<nsINode> GetPopupNode();
  already_AddRefed<nsINode> GetPopupLinkNode();
  already_AddRefed<nsIImageLoadingContent> GetPopupImageNode();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult GetContentSizeInternal(int32_t* aWidth, int32_t* aHeight,
                                  nscoord aMaxWidth, nscoord aMaxHeight);

  void PrepareToStartLoad(void);

  nsresult SyncParentSubDocMap();

  void RemoveFocusListener();
  void ReinitializeFocusListener();

  mozilla::dom::Selection* GetDocumentSelection();

  void DestroyPresShell();
  void DestroyPresContext();

  void InvalidatePotentialSubDocDisplayItem();

#ifdef NS_PRINTING
  // Called when the DocViewer is notified that the state
  // of Printing or PP has changed
  void SetIsPrintingInDocShellTree(nsIDocShellTreeItem* aParentNode,
                                   bool aIsPrintingOrPP, bool aStartAtTop);
#endif  // NS_PRINTING

  // Whether we should attach to the top level widget. This is true if we
  // are sharing/recycling a single base widget and not creating multiple
  // child widgets.
  bool ShouldAttachToTopLevel();

 protected:
  // Returns the current viewmanager.  Might be null.
  nsViewManager* GetViewManager();

  void DetachFromTopLevelWidget();

  // IMPORTANT: The ownership implicit in the following member
  // variables has been explicitly checked and set using nsCOMPtr
  // for owning pointers and raw COM interface pointers for weak
  // (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  WeakPtr<nsDocShell> mContainer;  // it owns me!
  nsWeakPtr mTopContainerWhilePrinting;
  RefPtr<nsDeviceContext> mDeviceContext;  // We create and own this baby

  // the following six items are explicitly in this order
  // so they will be destroyed in the reverse order (pinkerton, scc)
  nsCOMPtr<Document> mDocument;
  nsCOMPtr<nsIWidget> mWindow;  // may be null
  RefPtr<nsViewManager> mViewManager;
  RefPtr<nsPresContext> mPresContext;
  RefPtr<PresShell> mPresShell;

  RefPtr<nsDocViewerSelectionListener> mSelectionListener;
  RefPtr<nsDocViewerFocusListener> mFocusListener;

  nsCOMPtr<nsIContentViewer> mPreviousViewer;
  nsCOMPtr<nsISHEntry> mSHEntry;
  // Observer that will prevent bfcaching if it gets notified.  This
  // is non-null precisely when mSHEntry is non-null.
  RefPtr<BFCachePreventionObserver> mBFCachePreventionObserver;

  nsIWidget* mParentWidget;  // purposely won't be ref counted.  May be null
  bool mAttachedToParent;    // view is attached to the parent widget

  nsIntRect mBounds;

  // mTextZoom/mPageZoom record the textzoom/pagezoom of the first (galley)
  // presshell only.
  float mTextZoom;  // Text zoom, defaults to 1.0
  float mPageZoom;
  float mOverrideDPPX;  // DPPX overrided, defaults to 0.0

  int16_t mNumURLStarts;
  int16_t mDestroyBlockedCount;

  unsigned mStopped : 1;
  unsigned mLoaded : 1;
  unsigned mDeferredWindowClose : 1;
  // document management data
  //   these items are specific to markup documents (html and xml)
  //   may consider splitting these out into a subclass
  unsigned mIsSticky : 1;
  unsigned mInPermitUnload : 1;
  unsigned mInPermitUnloadPrompt : 1;

#ifdef NS_PRINTING
  unsigned mClosingWhilePrinting : 1;

#  if NS_PRINT_PREVIEW
  unsigned mPrintPreviewZoomed : 1;

  // These data members support delayed printing when the document is loading
  unsigned mPrintIsPending : 1;
  unsigned mPrintDocIsFullyLoaded : 1;
  nsCOMPtr<nsIPrintSettings> mCachedPrintSettings;
  nsCOMPtr<nsIWebProgressListener> mCachedPrintWebProgressListner;

  RefPtr<nsPrintJob> mPrintJob;
  float mOriginalPrintPreviewScale;
  float mPrintPreviewZoom;
  UniquePtr<AutoPrintEventDispatcher> mAutoBeforeAndAfterPrint;
#  endif  // NS_PRINT_PREVIEW

#endif  // NS_PRINTING

  /* character set member data */
  int32_t mHintCharsetSource;
  const Encoding* mHintCharset;
  const Encoding* mForceCharacterSet;

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
class AutoPrintEventDispatcher {
 public:
  explicit AutoPrintEventDispatcher(Document* aTop) : mTop(aTop) {
    DispatchEventToWindowTree(NS_LITERAL_STRING("beforeprint"));
  }
  ~AutoPrintEventDispatcher() {
    DispatchEventToWindowTree(NS_LITERAL_STRING("afterprint"));
  }

 private:
  static CallState CollectDocuments(Document& aDoc,
                                    nsTArray<nsCOMPtr<Document>>& aDocs) {
    aDocs.AppendElement(&aDoc);
    auto recurse = [&aDocs](Document& aSubDoc) {
      return CollectDocuments(aSubDoc, aDocs);
    };
    aDoc.EnumerateSubDocuments(recurse);
    return CallState::Continue;
  }

  void DispatchEventToWindowTree(const nsAString& aEvent) {
    nsTArray<nsCOMPtr<Document>> targets;
    if (mTop) {
      CollectDocuments(*mTop, targets);
    }
    for (nsCOMPtr<Document>& doc : targets) {
      nsContentUtils::DispatchTrustedEvent(doc, doc->GetWindow(), aEvent,
                                           CanBubble::eNo, Cancelable::eNo,
                                           nullptr);
    }
  }

  nsCOMPtr<Document> mTop;
};

}  // namespace mozilla

class nsDocumentShownDispatcher : public Runnable {
 public:
  explicit nsDocumentShownDispatcher(nsCOMPtr<Document> aDocument)
      : Runnable("nsDocumentShownDispatcher"), mDocument(aDocument) {}

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<Document> mDocument;
};

//------------------------------------------------------------------
// nsDocumentViewer
//------------------------------------------------------------------

//------------------------------------------------------------------
already_AddRefed<nsIContentViewer> NS_NewContentViewer() {
  RefPtr<nsDocumentViewer> viewer = new nsDocumentViewer();
  return viewer.forget();
}

void nsDocumentViewer::PrepareToStartLoad() {
  MOZ_DIAGNOSTIC_ASSERT(!GetIsPrintPreview(),
                        "Print preview tab should never navigate");

  mStopped = false;
  mLoaded = false;
  mAttachedToParent = false;
  mDeferredWindowClose = false;

#ifdef NS_PRINTING
  mPrintIsPending = false;
  mPrintDocIsFullyLoaded = false;
  mClosingWhilePrinting = false;

  // Make sure we have destroyed it and cleared the data member
  if (mPrintJob) {
    mPrintJob->Destroy();
    mPrintJob = nullptr;
  }

#endif  // NS_PRINTING
}

nsDocumentViewer::nsDocumentViewer()
    : mParentWidget(nullptr),
      mAttachedToParent(false),
      mTextZoom(1.0),
      mPageZoom(1.0),
      mOverrideDPPX(0.0),
      mNumURLStarts(0),
      mDestroyBlockedCount(0),
      mStopped(false),
      mLoaded(false),
      mDeferredWindowClose(false),
      mIsSticky(true),
      mInPermitUnload(false),
      mInPermitUnloadPrompt(false),
#ifdef NS_PRINTING
      mClosingWhilePrinting(false),
#  if NS_PRINT_PREVIEW
      mPrintPreviewZoomed(false),
      mPrintIsPending(false),
      mPrintDocIsFullyLoaded(false),
      mOriginalPrintPreviewScale(0.0),
      mPrintPreviewZoom(1.0),
#  endif  // NS_PRINT_PREVIEW
#endif    // NS_PRINTING
      mHintCharsetSource(kCharsetUninitialized),
      mHintCharset(nullptr),
      mForceCharacterSet(nullptr),
      mIsPageMode(false),
      mInitializedForPrintPreview(false),
      mHidden(false) {
  PrepareToStartLoad();
}

NS_IMPL_ADDREF(nsDocumentViewer)
NS_IMPL_RELEASE(nsDocumentViewer)

NS_INTERFACE_MAP_BEGIN(nsDocumentViewer)
  NS_INTERFACE_MAP_ENTRY(nsIContentViewer)
  NS_INTERFACE_MAP_ENTRY(nsIContentViewerEdit)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentViewerPrint)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentViewer)
#ifdef NS_PRINTING
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPrint)
#endif
NS_INTERFACE_MAP_END

nsDocumentViewer::~nsDocumentViewer() {
  if (mDocument) {
    Close(nullptr);
    mDocument->Destroy();
  }

#ifdef NS_PRINTING
  if (mPrintJob) {
    mPrintJob->Destroy();
    mPrintJob = nullptr;
  }
#endif

  MOZ_RELEASE_ASSERT(mDestroyBlockedCount == 0);
  NS_ASSERTION(!mPresShell && !mPresContext,
               "User did not call nsIContentViewer::Destroy");
  if (mPresShell || mPresContext) {
    // Make sure we don't hand out a reference to the content viewer to
    // the SHEntry!
    mSHEntry = nullptr;

    Destroy();
  }

  if (mSelectionListener) {
    mSelectionListener->Disconnect();
  }

  RemoveFocusListener();

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
/* virtual */
void nsDocumentViewer::LoadStart(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  if (!mDocument) {
    mDocument = aDocument;
  }
}

void nsDocumentViewer::RemoveFocusListener() {
  if (RefPtr<nsDocViewerFocusListener> oldListener =
          std::move(mFocusListener)) {
    oldListener->Disconnect();
    if (mDocument) {
      mDocument->RemoveEventListener(NS_LITERAL_STRING("focus"), oldListener,
                                     false);
      mDocument->RemoveEventListener(NS_LITERAL_STRING("blur"), oldListener,
                                     false);
    }
  }
}

void nsDocumentViewer::ReinitializeFocusListener() {
  RemoveFocusListener();
  mFocusListener = new nsDocViewerFocusListener(this);
  if (mDocument) {
    mDocument->AddEventListener(NS_LITERAL_STRING("focus"), mFocusListener,
                                false, false);
    mDocument->AddEventListener(NS_LITERAL_STRING("blur"), mFocusListener,
                                false, false);
  }
}

nsresult nsDocumentViewer::SyncParentSubDocMap() {
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
  docShell->GetInProcessParent(getter_AddRefs(parent));

  nsCOMPtr<nsPIDOMWindowOuter> parent_win =
      parent ? parent->GetWindow() : nullptr;
  if (!parent_win) {
    return NS_OK;
  }

  nsCOMPtr<Document> parent_doc = parent_win->GetDoc();
  if (!parent_doc) {
    return NS_OK;
  }

  if (mDocument && parent_doc->GetSubDocumentFor(element) != mDocument &&
      parent_doc->EventHandlingSuppressed()) {
    mDocument->SuppressEventHandling(parent_doc->EventHandlingSuppressed());
  }
  return parent_doc->SetSubDocumentFor(element, mDocument);
}

NS_IMETHODIMP
nsDocumentViewer::SetContainer(nsIDocShell* aContainer) {
  mContainer = static_cast<nsDocShell*>(aContainer);

  // We're loading a new document into the window where this document
  // viewer lives, sync the parent document's frame element -> sub
  // document map

  return SyncParentSubDocMap();
}

NS_IMETHODIMP
nsDocumentViewer::GetContainer(nsIDocShell** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsIDocShell> container(mContainer);
  container.swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Init(nsIWidget* aParentWidget, const nsIntRect& aBounds,
                       WindowGlobalChild* aActor) {
  return InitInternal(aParentWidget, nullptr, aActor, aBounds, true);
}

nsresult nsDocumentViewer::InitPresentationStuff(bool aDoInitialReflow) {
  // We assert this because initializing the pres shell could otherwise cause
  // re-entrancy into nsDocumentViewer methods, which might cause a different
  // pres shell to be created.  Callers of InitPresentationStuff should ensure
  // the call is appropriately bounded by an nsAutoScriptBlocker to decide
  // when it is safe for these re-entrant calls to be made.
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "InitPresentationStuff must only be called when scripts are "
             "blocked");

  if (GetIsPrintPreview()) return NS_OK;

  NS_ASSERTION(!mPresShell, "Someone should have destroyed the presshell!");

  // Now make the shell for the document
  mPresShell = mDocument->CreatePresShell(mPresContext, mViewManager);
  if (!mPresShell) {
    return NS_ERROR_FAILURE;
  }

  if (aDoInitialReflow) {
    // Since Initialize() will create frames for *all* items
    // that are currently in the document tree, we need to flush
    // any pending notifications to prevent the content sink from
    // duplicating layout frames for content it has added to the tree
    // but hasn't notified the document about. (Bug 154018)
    //
    // Note that we are flushing before we add mPresShell as an observer
    // to avoid bogus notifications.
    mDocument->FlushPendingNotifications(FlushType::ContentAndNotify);
  }

  mPresShell->BeginObservingDocument();

  // Initialize our view manager

  {
    int32_t p2a = mPresContext->AppUnitsPerDevPixel();
    MOZ_ASSERT(
        p2a ==
        mPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());

    nscoord width = p2a * mBounds.width;
    nscoord height = p2a * mBounds.height;

    mViewManager->SetWindowDimensions(width, height);
    mPresContext->SetVisibleArea(nsRect(0, 0, width, height));
    mPresContext->SetTextZoom(mTextZoom);
    mPresContext->SetFullZoom(mPageZoom);
    mPresContext->SetOverrideDPPX(mOverrideDPPX);
  }

  if (mWindow && mDocument->IsTopLevelContentDocument()) {
    // Set initial safe area insets
    ScreenIntMargin windowSafeAreaInsets;
    LayoutDeviceIntRect windowRect = mWindow->GetScreenBounds();
    nsCOMPtr<nsIScreen> screen = mWindow->GetWidgetScreen();
    if (screen) {
      windowSafeAreaInsets = nsContentUtils::GetWindowSafeAreaInsets(
          screen, mWindow->GetSafeAreaInsets(), windowRect);
    }

    mPresContext->SetSafeAreaInsets(windowSafeAreaInsets);
  }

  if (aDoInitialReflow) {
    RefPtr<PresShell> presShell = mPresShell;
    // Initial reflow
    presShell->Initialize();
  }

  // now register ourselves as a selection listener, so that we get
  // called when the selection changes in the window
  if (!mSelectionListener) {
    mSelectionListener = new nsDocViewerSelectionListener(this);
  }

  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  selection->AddSelectionListener(mSelectionListener);

  ReinitializeFocusListener();

  if (aDoInitialReflow && mDocument) {
    nsCOMPtr<Document> document = mDocument;
    document->ScrollToRef();
  }

  return NS_OK;
}

static nsPresContext* CreatePresContext(Document* aDocument,
                                        nsPresContext::nsPresContextType aType,
                                        nsView* aContainerView) {
  if (aContainerView) return new nsPresContext(aDocument, aType);
  return new nsRootPresContext(aDocument, aType);
}

//-----------------------------------------------
// This method can be used to initial the "presentation"
// The aDoCreation indicates whether it should create
// all the new objects or just initialize the existing ones
nsresult nsDocumentViewer::InitInternal(
    nsIWidget* aParentWidget, nsISupports* aState, WindowGlobalChild* aActor,
    const nsIntRect& aBounds, bool aDoCreation, bool aNeedMakeCX /*= true*/,
    bool aForceSetNewDocument /* = true*/) {
  // We don't want any scripts to run here. That can cause flushing,
  // which can cause reentry into initialization of this document viewer,
  // which would be disastrous.
  nsAutoScriptBlocker blockScripts;

  mParentWidget = aParentWidget;  // not ref counted
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
          mDocument->GetDisplayDocument()->GetPresShell()))) {
      // Create presentation context
      if (mIsPageMode) {
        // Presentation context already created in SetPageModeForTesting which
        // is calling this method
      } else {
        mPresContext = CreatePresContext(
            mDocument, nsPresContext::eContext_Galley, containerView);
      }
      NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

      nsresult rv = mPresContext->Init(mDeviceContext);
      if (NS_FAILED(rv)) {
        mPresContext = nullptr;
        return rv;
      }

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
      makeCX = !GetIsPrintPreview() &&
               aNeedMakeCX;  // needs to be true except when we are already in
                             // PP or we are enabling/disabling paginated mode.
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
    // Set script-context-owner in the document

    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(requestor);

    if (window) {
      nsCOMPtr<Document> curDoc = window->GetExtantDoc();
      if (aForceSetNewDocument || curDoc != mDocument) {
        rv = window->SetNewDocument(mDocument, aState, false, aActor);
        if (NS_FAILED(rv)) {
          Destroy();
          return rv;
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

void nsDocumentViewer::SetNavigationTiming(nsDOMNavigationTiming* timing) {
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
nsDocumentViewer::LoadComplete(nsresult aStatus) {
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
    RefPtr<PresShell> presShell = mPresShell;
    presShell->FlushPendingNotifications(FlushType::Layout);
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
  if (window &&
      (NS_SUCCEEDED(aStatus) || aStatus == NS_ERROR_PARSED_DATA_CACHED)) {
    // If this code changes, the code in nsDocLoader::DocLoaderIsEmpty
    // that fires load events for document.open() cases might need to
    // be updated too.
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetEvent event(true, eLoad);
    event.mFlags.mBubbles = false;
    event.mFlags.mCancelable = false;
    // XXX Dispatching to |window|, but using |document| as the target.
    event.mTarget = mDocument;

    // If the document presentation is being restored, we don't want to fire
    // onload to the document content since that would likely confuse scripts
    // on the page.

    nsIDocShell* docShell = window->GetDocShell();
    NS_ENSURE_TRUE(docShell, NS_ERROR_UNEXPECTED);

    // Unfortunately, docShell->GetRestoringDocument() might no longer be set
    // correctly.  In particular, it can be false by now if someone took it upon
    // themselves to block onload from inside restoration and unblock it later.
    // But we can detect the restoring case very simply: by whether our
    // document's readyState is COMPLETE.
    restoring =
        (mDocument->GetReadyStateEnum() == Document::READYSTATE_COMPLETE);
    if (!restoring) {
      NS_ASSERTION(
          mDocument->GetReadyStateEnum() == Document::READYSTATE_INTERACTIVE ||
              // test_stricttransportsecurity.html has old-style
              // docshell-generated about:blank docs reach this code!
              (mDocument->GetReadyStateEnum() ==
                   Document::READYSTATE_UNINITIALIZED &&
               NS_IsAboutBlank(mDocument->GetDocumentURI())),
          "Bad readystate");
#ifdef DEBUG
      bool docShellThinksWeAreRestoring;
      docShell->GetRestoringDocument(&docShellThinksWeAreRestoring);
      MOZ_ASSERT(!docShellThinksWeAreRestoring,
                 "How can docshell think we are restoring if we don't have a "
                 "READYSTATE_COMPLETE document?");
#endif  // DEBUG
      nsCOMPtr<Document> d = mDocument;
      mDocument->SetReadyStateInternal(Document::READYSTATE_COMPLETE);

      RefPtr<nsDOMNavigationTiming> timing(d->GetNavigationTiming());
      if (timing) {
        timing->NotifyLoadEventStart();
      }

      // Dispatch observer notification to notify observers document load is
      // complete.
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        nsIPrincipal* principal = d->NodePrincipal();
        os->NotifyObservers(ToSupports(d),
                            principal->IsSystemPrincipal()
                                ? "chrome-document-loaded"
                                : "content-document-loaded",
                            nullptr);
      }

      // Notify any devtools about the load.
      RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

      if (timelines && timelines->HasConsumer(docShell)) {
        timelines->AddMarkerForDocShell(
            docShell, MakeUnique<DocLoadingTimelineMarker>("document::Load"));
      }

      nsPIDOMWindowInner* innerWindow = window->GetCurrentInnerWindow();
      RefPtr<DocGroup> docGroup = d->GetDocGroup();
      // It is possible that the parent document's load event fires earlier than
      // childs' load event, and in this case we need to fire some artificial
      // load events to make the parent thinks the load events for child has
      // been done
      if (innerWindow && DocGroup::TryToLoadIframesInBackground()) {
        nsTArray<nsCOMPtr<nsIDocShell>> docShells;
        nsCOMPtr<nsIDocShell> container(mContainer);
        if (container) {
          int32_t count;
          container->GetInProcessChildCount(&count);
          // We first find all background loading iframes that need to
          // fire artificial load events, and instead of firing them as
          // soon as we find them, we store them in an array, to prevent
          // us from skipping some events.
          for (int32_t i = 0; i < count; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> child;
            container->GetInProcessChildAt(i, getter_AddRefs(child));
            nsCOMPtr<nsIDocShell> childIDocShell = do_QueryInterface(child);
            RefPtr<nsDocShell> docShell = nsDocShell::Cast(childIDocShell);
            if (docShell && docShell->TreatAsBackgroundLoad() &&
                docShell->GetDocument()->GetReadyStateEnum() <
                    Document::READYSTATE_COMPLETE) {
              docShells.AppendElement(childIDocShell);
            }
          }

          // Re-iterate the stored docShells to fire artificial load events
          for (size_t i = 0; i < docShells.Length(); ++i) {
            RefPtr<nsDocShell> docShell = nsDocShell::Cast(docShells[i]);
            if (docShell && docShell->TreatAsBackgroundLoad() &&
                docShell->GetDocument()->GetReadyStateEnum() <
                    Document::READYSTATE_COMPLETE) {
              nsEventStatus status = nsEventStatus_eIgnore;
              WidgetEvent event(true, eLoad);
              event.mFlags.mBubbles = false;
              event.mFlags.mCancelable = false;

              nsCOMPtr<nsPIDOMWindowOuter> win = docShell->GetWindow();
              nsCOMPtr<Element> element = win->GetFrameElementInternal();

              docShell->SetFakeOnLoadDispatched();
              EventDispatcher::Dispatch(element, nullptr, &event, nullptr,
                                        &status);
            }
          }
        }
      }

      d->SetLoadEventFiring(true);
      EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
      d->SetLoadEventFiring(false);

      RefPtr<nsDocShell> dShell = nsDocShell::Cast(docShell);
      if (docGroup && dShell->TreatAsBackgroundLoad()) {
        docGroup->TryFlushIframePostMessages(dShell->GetOuterWindowID());
      }

      if (timing) {
        timing->NotifyLoadEventEnd();
      }

      if (innerWindow) {
        innerWindow->QueuePerformanceNavigationTiming();
      }
    }
  } else {
    // XXX: Should fire error event to the document...
  }

  // Notify the document that it has been shown (regardless of whether
  // it was just loaded). Note: mDocument may be null now if the above
  // firing of onload caused the document to unload. Or, mDocument may not be
  // the "current active" document, if the above firing of onload caused our
  // docshell to navigate away. NOTE: In this latter scenario, it's likely that
  // we fired pagehide (when navigating away) without ever having fired
  // pageshow, and that's pretty broken... Fortunately, this should be rare.
  // (It requires us to spin the event loop in onload handler, e.g. via sync
  // XHR, in order for the navigation-away to happen before onload completes.)
  if (mDocument && mDocument->IsCurrentActiveDocument()) {
    // Re-get window, since it might have changed during above firing of onload
    window = mDocument->GetWindow();
    if (window) {
      nsIDocShell* docShell = window->GetDocShell();
      bool isInUnload;
      if (docShell && NS_SUCCEEDED(docShell->GetIsInUnload(&isInUnload)) &&
          !isInUnload) {
        mDocument->OnPageShow(restoring, nullptr);
      }
    }
  }

  if (!mStopped) {
    if (mDocument) {
      nsCOMPtr<Document> document = mDocument;
      document->ScrollToRef();
    }

    // Now that the document has loaded, we can tell the presshell
    // to unsuppress painting.
    if (mPresShell) {
      RefPtr<PresShell> presShell = mPresShell;
      presShell->UnsuppressPainting();
      // mPresShell could have been removed now, see bug 378682/421432
      if (mPresShell) {
        mPresShell->LoadComplete();
      }
    }
  }
  // Release the JS bytecode cache from its wait on the load event, and
  // potentially dispatch the encoding of the bytecode.
  if (mDocument && mDocument->ScriptLoader()) {
    mDocument->ScriptLoader()->LoadEventFired();
  }

  // It's probably a good idea to GC soon since we have finished loading.
  nsJSContext::PokeGC(
      JS::GCReason::LOAD_END,
      mDocument ? mDocument->GetWrapperPreserveColor() : nullptr);

#ifdef NS_PRINTING
  // Check to see if someone tried to print during the load
  if (mPrintIsPending) {
    mPrintIsPending = false;
    mPrintDocIsFullyLoaded = true;
    Print(mCachedPrintSettings, mCachedPrintWebProgressListner);
    mCachedPrintSettings = nullptr;
    mCachedPrintWebProgressListner = nullptr;
  }
#endif

  return rv;
}

bool nsDocumentViewer::GetLoadCompleted() { return mLoaded; }

bool nsDocumentViewer::GetIsStopped() { return mStopped; }

NS_IMETHODIMP
nsDocumentViewer::PermitUnload(uint32_t aPermitUnloadFlags,
                               bool* aPermitUnload) {
  return PermitUnloadInternal(&aPermitUnloadFlags, aPermitUnload);
}

nsresult nsDocumentViewer::PermitUnloadInternal(uint32_t* aPermitUnloadFlags,
                                                bool* aPermitUnload) {
  AutoDontWarnAboutSyncXHR disableSyncXHRWarning;

  nsresult rv = NS_OK;
  *aPermitUnload = true;

  if (!mDocument || mInPermitUnload || mInPermitUnloadPrompt) {
    return NS_OK;
  }

  // First, get the script global object from the document...
  nsPIDOMWindowOuter* window = mDocument->GetWindow();

  if (!window) {
    // This is odd, but not fatal
    NS_WARNING("window not set for document!");
    return NS_OK;
  }

  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "This is unsafe");

  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#prompt-to-unload-a-document
  // Create an RAII object on mDocument that will increment the
  // should-ignore-opens-during-unload counter on initialization
  // and decrement it again when it goes out of score (regardless
  // of how we exit this function).
  IgnoreOpensDuringUnload ignoreOpens(mDocument);

  // Now, fire an BeforeUnload event to the document and see if it's ok
  // to unload...
  nsPresContext* presContext = mDocument->GetPresContext();
  RefPtr<BeforeUnloadEvent> event =
      new BeforeUnloadEvent(mDocument, presContext, nullptr);
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
    AutoPopupStatePusher popupStatePusher(PopupBlocker::openAbused, true);

    // Never permit dialogs from the beforeunload handler
    nsGlobalWindowOuter* globalWindow = nsGlobalWindowOuter::Cast(window);
    dialogsAreEnabled = globalWindow->AreDialogsEnabled();
    nsGlobalWindowOuter::TemporarilyDisableDialogs disableDialogs(globalWindow);

    Document::PageUnloadingEventTimeStamp timestamp(mDocument);

    mInPermitUnload = true;
    EventDispatcher::DispatchDOMEvent(window, nullptr, event, mPresContext,
                                      nullptr);
    mInPermitUnload = false;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  nsAutoString text;
  event->GetReturnValue(text);

  if (StaticPrefs::dom_disable_beforeunload()) {
    *aPermitUnloadFlags = eDontPromptAndUnload;
  }

  // NB: we nullcheck mDocument because it might now be dead as a result of
  // the event being dispatched.
  if (*aPermitUnloadFlags != eDontPromptAndUnload && dialogsAreEnabled &&
      mDocument && !(mDocument->GetSandboxFlags() & SANDBOXED_MODALS) &&
      (!StaticPrefs::dom_require_user_interaction_for_beforeunload() ||
       mDocument->UserHasInteracted()) &&
      (event->WidgetEventPtr()->DefaultPrevented() || !text.IsEmpty())) {
    // If the consumer wants prompt requests to just stop unloading, we don't
    // need to prompt and can return immediately.
    if (*aPermitUnloadFlags == eDontPromptAndDontUnload) {
      *aPermitUnload = false;
      return NS_OK;
    }

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

      nsAutoString title, message, stayLabel, leaveLabel;
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              "OnBeforeUnloadTitle", title);
      nsresult tmp = nsContentUtils::GetLocalizedString(
          nsContentUtils::eDOM_PROPERTIES, "OnBeforeUnloadMessage", message);
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

      if (NS_FAILED(rv)) {
        NS_ERROR("Failed to get strings from dom.properties!");
        return NS_OK;
      }

      // Although the exact value is ignored, we must not pass invalid
      // bool values through XPConnect.
      bool dummy = false;
      int32_t buttonPressed = 0;
      uint32_t buttonFlags =
          (nsIPrompt::BUTTON_POS_0_DEFAULT |
           (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) |
           (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_1));

      nsAutoSyncOperation sync(mDocument);
      mInPermitUnloadPrompt = true;
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_COUNT, 1);
      rv = prompt->ConfirmEx(title.get(), message.get(), buttonFlags,
                             leaveLabel.get(), stayLabel.get(), nullptr,
                             nullptr, &dummy, &buttonPressed);
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
        mozilla::Telemetry::Accumulate(
            mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_ACTION, 2);
        *aPermitUnload = false;
        return NS_OK;
      }

      // Button 0 == leave, button 1 == stay
      *aPermitUnload = (buttonPressed == 0);
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::ONBEFOREUNLOAD_PROMPT_ACTION,
          (*aPermitUnload ? 1 : 0));
      // If the user decided to go ahead, make sure not to prompt the user again
      // by toggling the internal prompting bool to false:
      if (*aPermitUnload) {
        *aPermitUnloadFlags = eDontPromptAndUnload;
      }
    }
  }

  if (docShell) {
    int32_t childCount;
    docShell->GetInProcessChildCount(&childCount);

    for (int32_t i = 0; i < childCount && *aPermitUnload; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> item;
      docShell->GetInProcessChildAt(i, getter_AddRefs(item));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(item));

      if (docShell) {
        nsCOMPtr<nsIContentViewer> cv;
        docShell->GetContentViewer(getter_AddRefs(cv));

        if (cv) {
          cv->PermitUnloadInternal(aPermitUnloadFlags, aPermitUnload);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetBeforeUnloadFiring(bool* aInEvent) {
  *aInEvent = mInPermitUnload;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetInPermitUnload(bool* aInEvent) {
  *aInEvent = mInPermitUnloadPrompt;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::PageHide(bool aIsUnload) {
  AutoDontWarnAboutSyncXHR disableSyncXHRWarning;

  mHidden = true;

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIsUnload) {
    // Poke the GC. The window might be collectable garbage now.
    nsJSContext::PokeGC(JS::GCReason::PAGE_HIDE,
                        mDocument->GetWrapperPreserveColor(),
                        StaticPrefs::javascript_options_gc_delay() * 2);
  }

  mDocument->OnPageHide(!aIsUnload, nullptr);

  // inform the window so that the focus state is reset.
  NS_ENSURE_STATE(mDocument);
  nsPIDOMWindowOuter* window = mDocument->GetWindow();
  if (window) window->PageHidden();

  if (aIsUnload) {
    // if Destroy() was called during OnPageHide(), mDocument is nullptr.
    NS_ENSURE_STATE(mDocument);

    // First, get the window from the document...
    nsPIDOMWindowOuter* window = mDocument->GetWindow();

    if (!window) {
      // Fail if no window is available...
      NS_WARNING("window not set for document!");
      return NS_ERROR_NULL_POINTER;
    }

    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#unload-a-document
    // Create an RAII object on mDocument that will increment the
    // should-ignore-opens-during-unload counter on initialization
    // and decrement it again when it goes out of scope.
    IgnoreOpensDuringUnload ignoreOpens(mDocument);

    // Now, fire an Unload event to the document...
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetEvent event(true, eUnload);
    event.mFlags.mBubbles = false;
    // XXX Dispatching to |window|, but using |document| as the target.
    event.mTarget = mDocument;

    // Never permit popups from the unload handler, no matter how we get
    // here.
    AutoPopupStatePusher popupStatePusher(PopupBlocker::openAbused, true);

    Document::PageUnloadingEventTimeStamp timestamp(mDocument);

    EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
  }

#ifdef MOZ_XUL
  // look for open menupopups and close them after the unload event, in case
  // the unload event listeners open any new popups
  nsContentUtils::HidePopupsInDocument(mDocument);
#endif

  return NS_OK;
}

static void AttachContainerRecurse(nsIDocShell* aShell) {
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  if (viewer) {
    viewer->SetIsHidden(false);
    Document* doc = viewer->GetDocument();
    if (doc) {
      doc->SetContainer(static_cast<nsDocShell*>(aShell));
    }
    if (PresShell* presShell = viewer->GetPresShell()) {
      presShell->SetForwardingContainer(WeakPtr<nsDocShell>());
    }
  }

  // Now recurse through the children
  int32_t childCount;
  aShell->GetInProcessChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    aShell->GetInProcessChildAt(i, getter_AddRefs(childItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(childItem);
    AttachContainerRecurse(shell);
  }
}

NS_IMETHODIMP
nsDocumentViewer::Open(nsISupports* aState, nsISHEntry* aSHEntry) {
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);

  if (mDocument) {
    mDocument->SetContainer(mContainer);
  }

  nsresult rv = InitInternal(mParentWidget, aState, nullptr, mBounds, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mHidden = false;

  if (mPresShell) mPresShell->SetForwardingContainer(WeakPtr<nsDocShell>());

  // Rehook the child presentations.  The child shells are still in
  // session history, so get them from there.

  if (aSHEntry) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    int32_t itemIndex = 0;
    while (NS_SUCCEEDED(
               aSHEntry->ChildShellAt(itemIndex++, getter_AddRefs(item))) &&
           item) {
      nsCOMPtr<nsIDocShell> shell = do_QueryInterface(item);
      AttachContainerRecurse(shell);
    }
  }

  SyncParentSubDocMap();

  ReinitializeFocusListener();

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

    nsViewManager* vm = GetViewManager();
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
nsDocumentViewer::Close(nsISHEntry* aSHEntry) {
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

  if (!mDocument) return NS_OK;

  if (mSHEntry) {
    if (mBFCachePreventionObserver) {
      mBFCachePreventionObserver->Disconnect();
    }
    mBFCachePreventionObserver = new BFCachePreventionObserver(mDocument);
    mDocument->AddMutationObserver(mBFCachePreventionObserver);
  }

#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // Turn scripting back on
  // after PrintPreview had turned it off
  if (GetIsPrintPreview() && mPrintJob) {
    mPrintJob->TurnScriptingOn(true);
  }
#endif

#ifdef NS_PRINTING
  // A Close was called while we were printing
  // so don't clear the ScriptGlobalObject
  // or clear the mDocument below
  if (mPrintJob && !mClosingWhilePrinting) {
    mClosingWhilePrinting = true;
  } else
#endif
  {
    // out of band cleanup of docshell
    mDocument->SetScriptGlobalObject(nullptr);

    if (!mSHEntry && mDocument) mDocument->RemovedFromDocShell();
  }

  RemoveFocusListener();
  return NS_OK;
}

static void DetachContainerRecurse(nsIDocShell* aShell) {
  // Unhook this docshell's presentation
  aShell->SynchronizeLayoutHistoryState();
  nsCOMPtr<nsIContentViewer> viewer;
  aShell->GetContentViewer(getter_AddRefs(viewer));
  if (viewer) {
    if (Document* doc = viewer->GetDocument()) {
      doc->SetContainer(nullptr);
    }
    if (PresShell* presShell = viewer->GetPresShell()) {
      auto weakShell = static_cast<nsDocShell*>(aShell);
      presShell->SetForwardingContainer(weakShell);
    }
  }

  // Now recurse through the children
  int32_t childCount;
  aShell->GetInProcessChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> childItem;
    aShell->GetInProcessChildAt(i, getter_AddRefs(childItem));
    nsCOMPtr<nsIDocShell> shell = do_QueryInterface(childItem);
    DetachContainerRecurse(shell);
  }
}

NS_IMETHODIMP
nsDocumentViewer::Destroy() {
  NS_ASSERTION(mDocument, "No document in Destroy()!");

  // Don't let the document get unloaded while we are printing.
  // this could happen if we hit the back button during printing.
  // We also keep the viewer from being cached in session history, since
  // we require all documents there to be sanitized.
  if (mDestroyBlockedCount != 0) {
    return NS_OK;
  }

#ifdef NS_PRINTING
  // Here is where we check to see if the document was still being prepared
  // for printing when it was asked to be destroy from someone externally
  // This usually happens if the document is unloaded while the user is in the
  // Print Dialog
  //
  // So we flip the bool to remember that the document is going away
  // and we can clean up and abort later after returning from the Print Dialog
  if (mPrintJob) {
    if (mPrintJob->CheckBeforeDestroy()) {
      return NS_OK;
    }
  }
  // Dispatch the 'afterprint' event now, if pending:
  mAutoBeforeAndAfterPrint = nullptr;
#endif

  // We want to make sure to disconnect mBFCachePreventionObserver before we
  // Sanitize() below.
  if (mBFCachePreventionObserver) {
    mBFCachePreventionObserver->Disconnect();
    mBFCachePreventionObserver = nullptr;
  }

  if (mSHEntry && mDocument && !mDocument->IsBFCachingAllowed()) {
    // Just drop the SHEntry now and pretend like we never even tried to bfcache
    // this viewer.  This should only happen when someone calls
    // DisallowBFCaching() after CanSavePresentation() already ran.  Ensure that
    // the SHEntry has no viewer and its state is synced up.  We want to do this
    // via a stack reference, in case those calls mess with our members.
    nsCOMPtr<nsISHEntry> shEntry = std::move(mSHEntry);
    shEntry->SetContentViewer(nullptr);
    shEntry->SyncPresentationState();
  }

  // If we were told to put ourselves into session history instead of destroy
  // the presentation, do that now.
  if (mSHEntry) {
    if (mPresShell) mPresShell->Freeze();

    // Make sure the presentation isn't torn down by Hide().
    mSHEntry->SetSticky(mIsSticky);
    mIsSticky = true;

    // Remove our root view from the view hierarchy.
    if (mPresShell) {
      nsViewManager* vm = mPresShell->GetViewManager();
      if (vm) {
        nsView* rootView = vm->GetRootView();

        if (rootView) {
          nsView* rootViewParent = rootView->GetParent();
          if (rootViewParent) {
            nsView* subdocview = rootViewParent->GetParent();
            if (subdocview) {
              nsIFrame* f = subdocview->GetFrame();
              if (f) {
                nsSubDocumentFrame* s = do_QueryFrame(f);
                if (s) {
                  s->ClearDisplayItems();
                }
              }
            }
            nsViewManager* parentVM = rootViewParent->GetViewManager();
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
    nsCOMPtr<nsISHEntry> shEntry =
        std::move(mSHEntry);  // we'll need this below

    shEntry->SetContentViewer(this);

    // Always sync the presentation state.  That way even if someone screws up
    // and shEntry has no window state at this point we'll be ok; we just won't
    // cache ourselves.
    shEntry->SyncPresentationState();
    shEntry->SynchronizeLayoutHistoryState();

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
    if (mPresShell) {
      mPresShell->SetForwardingContainer(mContainer);
    }

    // Do the same for our children.  Note that we need to get the child
    // docshells from the SHEntry now; the docshell will have cleared them.
    nsCOMPtr<nsIDocShellTreeItem> item;
    int32_t itemIndex = 0;
    while (NS_SUCCEEDED(
               shEntry->ChildShellAt(itemIndex++, getter_AddRefs(item))) &&
           item) {
      nsCOMPtr<nsIDocShell> shell = do_QueryInterface(item);
      DetachContainerRecurse(shell);
    }

    return NS_OK;
  }

  // The document was not put in the bfcache

  // Protect against pres shell destruction running scripts and re-entrantly
  // creating a new presentation.
  nsAutoScriptBlocker scriptBlocker;

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
  if (mPrintJob) {
    RefPtr<nsPrintJob> printJob = std::move(mPrintJob);
#  ifdef NS_PRINT_PREVIEW
    if (printJob->IsDoingPrintPreview()) {
      printJob->FinishPrintPreview();
    }
#  endif
    printJob->Destroy();
    MOZ_ASSERT(!mPrintJob,
               "mPrintJob shouldn't be recreated while destroying it");
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
nsDocumentViewer::Stop(void) {
  NS_ASSERTION(mDocument, "Stop called too early or too late");
  if (mDocument) {
    mDocument->StopDocumentLoad();
  }

  if (!mHidden && (mLoaded || mStopped) && mPresContext && !mSHEntry)
    mPresContext->SetImageAnimationMode(imgIContainer::kDontAnimMode);

  mStopped = true;

  if (!mLoaded && mPresShell) {
    // Well, we might as well paint what we have so far.
    RefPtr<PresShell> presShell = mPresShell;  // bug 378682
    presShell->UnsuppressPainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetDOMDocument(Document** aResult) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<Document> document = mDocument;
  document.forget(aResult);
  return NS_OK;
}

Document* nsDocumentViewer::GetDocument() { return mDocument; }

nsresult nsDocumentViewer::SetDocument(Document* aDocument) {
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

  if (!aDocument) return NS_ERROR_NULL_POINTER;

  return SetDocumentInternal(aDocument, false);
}

NS_IMETHODIMP
nsDocumentViewer::SetDocumentInternal(Document* aDocument,
                                      bool aForceReuseInnerWindow) {
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
        node->GetInProcessChildCount(&count);
        for (int32_t i = 0; i < count; ++i) {
          nsCOMPtr<nsIDocShellTreeItem> child;
          node->GetInProcessChildAt(0, getter_AddRefs(child));
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
      nsresult rv =
          window->SetNewDocument(aDocument, nullptr, aForceReuseInnerWindow);
      if (NS_FAILED(rv)) {
        Destroy();
        return rv;
      }
    }
  }

  nsresult rv = SyncParentSubDocMap();
  NS_ENSURE_SUCCESS(rv, rv);

  // Replace the current pres shell with a new shell for the new document

  // Protect against pres shell destruction running scripts and re-entrantly
  // creating a new presentation.
  nsAutoScriptBlocker scriptBlocker;

  if (mPresShell) {
    DestroyPresShell();
  }

  if (mPresContext) {
    DestroyPresContext();

    mWindow = nullptr;
    rv = InitInternal(mParentWidget, nullptr, nullptr, mBounds, true, true,
                      false);
  }

  return rv;
}

PresShell* nsDocumentViewer::GetPresShell() { return mPresShell; }

nsPresContext* nsDocumentViewer::GetPresContext() { return mPresContext; }

nsViewManager* nsDocumentViewer::GetViewManager() { return mViewManager; }

NS_IMETHODIMP
nsDocumentViewer::GetBounds(nsIntRect& aResult) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  aResult = mBounds;
  return NS_OK;
}

nsIContentViewer* nsDocumentViewer::GetPreviousViewer() {
  return mPreviousViewer;
}

void nsDocumentViewer::SetPreviousViewer(nsIContentViewer* aViewer) {
  // NOTE:  |Show| sets |mPreviousViewer| to null without calling this
  // function.

  if (aViewer) {
    NS_ASSERTION(!mPreviousViewer,
                 "can't set previous viewer when there already is one");

    // In a multiple chaining situation (which occurs when running a thrashing
    // test like i-bench or jrgm's tests with no delay), we can build up a
    // whole chain of viewers.  In order to avoid this, we always set our
    // previous viewer to the MOST previous viewer in the chain, and then dump
    // the intermediate link from the chain.  This ensures that at most only 2
    // documents are alive and undestroyed at any given time (the one that is
    // showing and the one that is loading with painting suppressed). It's very
    // important that if this ever gets changed the code before the
    // RestorePresentation call in nsDocShell::InternalLoad be changed
    // accordingly.
    //
    // Make sure we hold a strong ref to prevViewer here, since we'll
    // tell aViewer to drop it.
    nsCOMPtr<nsIContentViewer> prevViewer = aViewer->GetPreviousViewer();
    if (prevViewer) {
      aViewer->SetPreviousViewer(nullptr);
      aViewer->Destroy();
      return SetPreviousViewer(prevViewer);
    }
  }

  mPreviousViewer = aViewer;
}

NS_IMETHODIMP
nsDocumentViewer::SetBoundsWithFlags(const nsIntRect& aBounds,
                                     uint32_t aFlags) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  bool boundsChanged = !mBounds.IsEqualEdges(aBounds);
  mBounds = aBounds;

  if (mWindow && !mAttachedToParent) {
    // Resize the widget, but don't trigger repaint. Layout will generate
    // repaint requests during reflow.
    mWindow->Resize(aBounds.x, aBounds.y, aBounds.width, aBounds.height, false);
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
    nscoord width = NSIntPixelsToAppUnits(mBounds.width, p2a);
    nscoord height = NSIntPixelsToAppUnits(mBounds.height, p2a);
    nsView* rootView = mViewManager->GetRootView();
    if (boundsChanged && rootView) {
      nsRect viewDims = rootView->GetDimensions();
      // If the view/frame tree and prescontext visible area already has the new
      // size but we did not, then it's likely that we got reflowed in response
      // to a call to GetContentSize. Thus there is a disconnect between the
      // size on the document viewer/docshell/containing widget and view
      // tree/frame tree/prescontext visible area). SetWindowDimensions compares
      // to the root view dimenstions to determine if it needs to do anything;
      // if they are the same as the new size it won't do anything, but we still
      // need to invalidate because what we want to draw to the screen has
      // changed.
      if (viewDims.width == width && viewDims.height == height) {
        nsIFrame* f = rootView->GetFrame();
        if (f) {
          f->InvalidateFrame();
        }
      }
    }

    mViewManager->SetWindowDimensions(
        width, height, !!(aFlags & nsIContentViewer::eDelayResize));
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
nsDocumentViewer::SetBounds(const nsIntRect& aBounds) {
  return SetBoundsWithFlags(aBounds, 0);
}

NS_IMETHODIMP
nsDocumentViewer::Move(int32_t aX, int32_t aY) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);
  mBounds.MoveTo(aX, aY);
  if (mWindow) {
    mWindow->Move(aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Show(void) {
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
      treeItem->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
      nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(root);
      RefPtr<ChildSHistory> history = webNav->GetSessionHistory();
      if (history) {
        int32_t prevIndex, loadedIndex;
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(treeItem);
        docShell->GetPreviousEntryIndex(&prevIndex);
        docShell->GetLoadedEntryIndex(&loadedIndex);
        MOZ_LOG(gPageCacheLog, LogLevel::Verbose,
                ("About to evict content viewers: prev=%d, loaded=%d",
                 prevIndex, loadedIndex));
        history->LegacySHistory()->EvictOutOfRangeContentViewers(loadedIndex);
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

  // Hold on to the document so we can use it after the script blocker below
  // has been released (which might re-entrantly call into other
  // nsDocumentViewer methods).
  nsCOMPtr<Document> document = mDocument;

  if (mDocument && !mPresShell) {
    // The InitPresentationStuff call below requires a script blocker, because
    // its PresShell::Initialize call can cause scripts to run and therefore
    // re-entrant calls to nsDocumentViewer methods to be made.
    nsAutoScriptBlocker scriptBlocker;

    NS_ASSERTION(!mWindow, "Window already created but no presshell?");

    nsCOMPtr<nsIBaseWindow> base_win(mContainer);
    if (base_win) {
      base_win->GetParentWidget(&mParentWidget);
      if (mParentWidget) {
        // GetParentWidget AddRefs, but mParentWidget is weak
        mParentWidget->Release();
      }
    }

    nsView* containerView = FindContainerView();

    nsresult rv = CreateDeviceContext(containerView);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create presentation context
    NS_ASSERTION(!mPresContext,
                 "Shouldn't have a prescontext if we have no shell!");
    mPresContext = CreatePresContext(mDocument, nsPresContext::eContext_Galley,
                                     containerView);
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);

    rv = mPresContext->Init(mDeviceContext);
    if (NS_FAILED(rv)) {
      mPresContext = nullptr;
      return rv;
    }

    rv = MakeWindow(nsSize(mPresContext->DevPixelsToAppUnits(mBounds.width),
                           mPresContext->DevPixelsToAppUnits(mBounds.height)),
                    containerView);
    if (NS_FAILED(rv)) return rv;

    if (mPresContext) {
      Hide();

      rv = InitPresentationStuff(mDocument->MayStartLayout());
    }

    // If we get here the document load has already started and the
    // window is shown because some JS on the page caused it to be
    // shown...

    if (mPresShell) {
      RefPtr<PresShell> presShell = mPresShell;  // bug 378682
      presShell->UnsuppressPainting();
    }
  }

  // Notify observers that a new page has been shown. This will get run
  // from the event loop after we actually draw the page.
  RefPtr<nsDocumentShownDispatcher> event =
      new nsDocumentShownDispatcher(document);
  document->Dispatch(TaskCategory::Other, event.forget());

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Hide(void) {
  if (!mAttachedToParent && mWindow) {
    mWindow->Show(false);
  }

  if (!mPresShell) return NS_OK;

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

  // Do not run ScriptRunners queued by DestroyPresShell() in the intermediate
  // state before we're done destroying PresShell, PresContext, ViewManager,
  // etc.
  nsAutoScriptBlocker scriptBlocker;

  DestroyPresShell();

  DestroyPresContext();

  mViewManager = nullptr;
  mWindow = nullptr;
  mDeviceContext = nullptr;
  mParentWidget = nullptr;

  nsCOMPtr<nsIBaseWindow> base_win(mContainer);

  if (base_win && !mAttachedToParent) {
    base_win->SetParentWidget(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetSticky(bool* aSticky) {
  *aSticky = mIsSticky;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetSticky(bool aSticky) {
  mIsSticky = aSticky;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::RequestWindowClose(bool* aCanClose) {
#ifdef NS_PRINTING
  if (mPrintIsPending || (mPrintJob && mPrintJob->GetIsPrinting())) {
    *aCanClose = false;
    mDeferredWindowClose = true;
  } else
#endif
    *aCanClose = true;

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::ClearHistoryEntry() {
  if (mDocument) {
    nsJSContext::PokeGC(JS::GCReason::PAGE_HIDE,
                        mDocument->GetWrapperPreserveColor(),
                        StaticPrefs::javascript_options_gc_delay() * 2);
  }

  mSHEntry = nullptr;
  return NS_OK;
}

//-------------------------------------------------------

nsresult nsDocumentViewer::MakeWindow(const nsSize& aSize,
                                      nsView* aContainerView) {
  if (GetIsPrintPreview()) return NS_OK;

  bool shouldAttach = ShouldAttachToTopLevel();

  if (shouldAttach) {
    // If the old view is already attached to our parent, detach
    DetachFromTopLevelWidget();
  }

  mViewManager = new nsViewManager();

  nsDeviceContext* dx = mPresContext->DeviceContext();

  nsresult rv = mViewManager->Init(dx);
  if (NS_FAILED(rv)) return rv;

  // The root view is always at 0,0.
  nsRect tbounds(nsPoint(0, 0), aSize);
  // Create a view
  nsView* view = mViewManager->CreateView(tbounds, aContainerView);
  if (!view) return NS_ERROR_OUT_OF_MEMORY;

  // Create a widget if we were given a parent widget or don't have a
  // container view that we can hook up to without a widget.
  // Don't create widgets for ResourceDocs (external resources & svg images),
  // because when they're displayed, they're painted into *another* document's
  // widget.
  if (!mDocument->IsResourceDoc() && (mParentWidget || !aContainerView)) {
    // pass in a native widget to be the parent widget ONLY if the view
    // hierarchy will stand alone. otherwise the view will find its own parent
    // widget and "do the right thing" to establish a parent/child widget
    // relationship
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
    } else if (!aContainerView && mParentWidget) {
      rv = view->CreateWidgetForParent(mParentWidget, initDataPtr, true, false);
    } else {
      rv = view->CreateWidget(initDataPtr, true, false);
    }
    if (NS_FAILED(rv)) return rv;
  }

  // Setup hierarchical relationship in view manager
  mViewManager->SetRootView(view);

  mWindow = view->GetWidget();

  // This SetFocus is necessary so the Arrow Key and Page Key events
  // go to the scrolled view as soon as the Window is created instead of going
  // to the browser window (this enables keyboard scrolling of the document)
  // mWindow->SetFocus();

  return rv;
}

void nsDocumentViewer::DetachFromTopLevelWidget() {
  if (mViewManager) {
    nsView* oldView = mViewManager->GetRootView();
    if (oldView && oldView->IsAttachedToTopLevel()) {
      oldView->DetachFromTopLevelWidget();
    }
  }
  mAttachedToParent = false;
}

nsView* nsDocumentViewer::FindContainerView() {
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

  nsIFrame* subdocFrame = containerElement->GetPrimaryFrame();
  if (!subdocFrame) {
    // XXX Silenced by default in bug 1175289
    LAYOUT_WARNING("Subdocument container has no frame");
    return nullptr;
  }

  // subdocFrame might not be a subdocument frame; the frame
  // constructor can treat a <frame> as an inline in some XBL
  // cases. Treat that as display:none, the document is not
  // displayed.
  if (!subdocFrame->IsSubDocumentFrame()) {
    NS_WARNING_ASSERTION(subdocFrame->Type() == LayoutFrameType::None,
                         "Subdocument container has non-subdocument frame");
    return nullptr;
  }

  NS_ASSERTION(subdocFrame->GetView(), "Subdoc frames must have views");
  return static_cast<nsSubDocumentFrame*>(subdocFrame)->EnsureInnerView();
}

nsresult nsDocumentViewer::CreateDeviceContext(nsView* aContainerView) {
  MOZ_ASSERT(!mPresShell && !mWindow,
             "This will screw up our existing presentation");
  MOZ_ASSERT(mDocument, "Gotta have a document here");

  Document* doc = mDocument->GetDisplayDocument();
  if (doc) {
    NS_ASSERTION(!aContainerView,
                 "External resource document embedded somewhere?");
    // We want to use our display document's device context if possible
    nsPresContext* ctx = doc->GetPresContext();
    if (ctx) {
      mDeviceContext = ctx->DeviceContext();
      return NS_OK;
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
mozilla::dom::Selection* nsDocumentViewer::GetDocumentSelection() {
  if (!mPresShell) {
    return nullptr;
  }

  return mPresShell->GetCurrentSelection(SelectionType::eNormal);
}

/* ============================================================================
 * nsIContentViewerEdit
 * ============================================================================
 */

NS_IMETHODIMP nsDocumentViewer::ClearSelection() {
  // use nsCopySupport::GetSelectionForCopy() ?
  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  selection->CollapseToStart(rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP nsDocumentViewer::SelectAll() {
  // XXX this is a temporary implementation copied from nsWebShell
  // for now. I think Document and friends should have some helper
  // functions to make this easier.

  // use nsCopySupport::GetSelectionForCopy() ?
  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> bodyNode;
  if (mDocument->IsHTMLOrXHTML()) {
    // XXXbz why not just do GetBody() for all documents, then GetRootElement()
    // if GetBody() is null?
    bodyNode = mDocument->GetBody();
  } else {
    bodyNode = mDocument->GetRootElement();
  }
  if (!bodyNode) return NS_ERROR_FAILURE;

  ErrorResult err;
  selection->RemoveAllRanges(err);
  if (err.Failed()) {
    return err.StealNSResult();
  }

  mozilla::dom::Selection::AutoUserInitiated userSelection(selection);
  selection->SelectAllChildren(*bodyNode, err);
  return err.StealNSResult();
}

NS_IMETHODIMP nsDocumentViewer::CopySelection() {
  RefPtr<PresShell> presShell = mPresShell;
  nsCopySupport::FireClipboardEvent(eCopy, nsIClipboard::kGlobalClipboard,
                                    presShell, nullptr);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::CopyLinkLocation() {
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsINode> node = GetPopupLinkNode();
  // make noise if we're not in a link
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsCOMPtr<dom::Element> elm(do_QueryInterface(node));
  NS_ENSURE_TRUE(elm, NS_ERROR_FAILURE);

  nsAutoString locationText;
  nsContentUtils::GetLinkLocation(elm, locationText);
  if (locationText.IsEmpty()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIClipboardHelper> clipboard(
      do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // copy the href onto the clipboard
  return clipboard->CopyString(locationText);
}

NS_IMETHODIMP nsDocumentViewer::CopyImage(int32_t aCopyFlags) {
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsIImageLoadingContent> node = GetPopupImageNode();
  // make noise if we're not in an image
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  nsCOMPtr<nsILoadContext> loadContext(mContainer);
  return nsCopySupport::ImageCopy(node, loadContext, aCopyFlags);
}

NS_IMETHODIMP nsDocumentViewer::GetCopyable(bool* aCopyable) {
  NS_ENSURE_ARG_POINTER(aCopyable);
  *aCopyable = nsCopySupport::CanCopy(mDocument);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetContents(const char* mimeType,
                                            bool selectionOnly,
                                            nsAString& aOutValue) {
  aOutValue.Truncate();

  NS_ENSURE_TRUE(mPresShell, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_INITIALIZED);

  // Now we have the selection.  Make sure it's nonzero:
  RefPtr<Selection> sel;
  if (selectionOnly) {
    sel = nsCopySupport::GetSelectionForCopy(mDocument);
    NS_ENSURE_TRUE(sel, NS_ERROR_FAILURE);

    if (sel->IsCollapsed()) {
      return NS_OK;
    }
  }

  // call the copy code
  return nsCopySupport::GetContents(nsDependentCString(mimeType), 0, sel,
                                    mDocument, aOutValue);
}

NS_IMETHODIMP nsDocumentViewer::GetCanGetContents(bool* aCanGetContents) {
  NS_ENSURE_ARG_POINTER(aCanGetContents);
  *aCanGetContents = false;
  NS_ENSURE_STATE(mDocument);
  *aCanGetContents = nsCopySupport::CanCopy(mDocument);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::SetCommandNode(nsINode* aNode) {
  Document* document = GetDocument();
  NS_ENSURE_STATE(document);

  nsCOMPtr<nsPIDOMWindowOuter> window(document->GetWindow());
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_STATE(root);

  root->SetPopupNode(aNode);
  return NS_OK;
}

void nsDocumentViewer::PropagateToPresContextsHelper(CallChildFunc aChildFunc,
                                                     PresContextFunc aPcFunc) {
  CallChildren(aChildFunc);

  if (mDocument) {
    auto resourceDoc = [aPcFunc](Document& aResourceDoc) {
      if (nsPresContext* pc = aResourceDoc.GetPresContext()) {
        aPcFunc(pc);
      }
      return CallState::Continue;
    };
    mDocument->EnumerateExternalResources(resourceDoc);
  }

  if (mPresContext) {
    aPcFunc(mPresContext);
  }
}

void nsDocumentViewer::CallChildren(CallChildFunc aFunc) {
  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (!docShell) {
    return;
  }
  int32_t n = 0;
  docShell->GetInProcessChildCount(&n);
  for (int32_t i = 0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    docShell->GetInProcessChildAt(i, getter_AddRefs(child));
    nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
    NS_ASSERTION(childAsShell, "null child in docshell");
    if (childAsShell) {
      nsCOMPtr<nsIContentViewer> childCV;
      childAsShell->GetContentViewer(getter_AddRefs(childCV));
      if (childCV) {
        aFunc(static_cast<nsDocumentViewer*>(childCV.get()));
      }
    }
  }
}

NS_IMETHODIMP
nsDocumentViewer::SetTextZoom(float aTextZoom) {
  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  if (GetIsPrintPreview()) {
    return NS_OK;
  }

  bool textZoomChange = (mTextZoom != aTextZoom);
  mTextZoom = aTextZoom;

  auto childFn = [aTextZoom](nsDocumentViewer* aChild) {
    aChild->SetTextZoom(aTextZoom);
  };
  auto presContextFn = [aTextZoom](nsPresContext* aPc) {
    aPc->SetTextZoom(aTextZoom);
  };
  PropagateToPresContextsHelper(childFn, presContextFn);

  // Dispatch TextZoomChange event only if text zoom value has changed.
  if (textZoomChange) {
    nsContentUtils::DispatchChromeEvent(mDocument, ToSupports(mDocument),
                                        NS_LITERAL_STRING("TextZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetTextZoom(float* aTextZoom) {
  NS_ENSURE_ARG_POINTER(aTextZoom);
  nsPresContext* pc = GetPresContext();
  *aTextZoom = pc ? pc->TextZoom() : 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetEffectiveTextZoom(float* aEffectiveTextZoom) {
  NS_ENSURE_ARG_POINTER(aEffectiveTextZoom);
  nsPresContext* pc = GetPresContext();
  *aEffectiveTextZoom = pc ? pc->EffectiveTextZoom() : 1.0f;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetFullZoom(float aFullZoom) {
#ifdef NS_PRINT_PREVIEW
  if (GetIsPrintPreview()) {
    nsPresContext* pc = GetPresContext();
    NS_ENSURE_TRUE(pc, NS_OK);
    PresShell* presShell = pc->GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_OK);

    if (!mPrintPreviewZoomed) {
      mOriginalPrintPreviewScale = pc->GetPrintPreviewScale();
      mPrintPreviewZoomed = true;
    }

    mPrintPreviewZoom = aFullZoom;
    pc->SetPrintPreviewScale(aFullZoom * mOriginalPrintPreviewScale);
    nsPageSequenceFrame* pf = presShell->GetPageSequenceFrame();
    if (pf) {
      nsIFrame* f = do_QueryFrame(pf);
      presShell->FrameNeedsReflow(f, IntrinsicDirty::Resize, NS_FRAME_IS_DIRTY);
    }

    nsIFrame* rootFrame = presShell->GetRootFrame();
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

  // Dispatch PreFullZoomChange event only if fullzoom value really has changed.
  if (fullZoomChange) {
    nsContentUtils::DispatchChromeEvent(mDocument, ToSupports(mDocument),
                                        NS_LITERAL_STRING("PreFullZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
  }

  mPageZoom = aFullZoom;

  auto childFn = [aFullZoom](nsDocumentViewer* aChild) {
    aChild->SetFullZoom(aFullZoom);
  };
  auto presContextFn = [aFullZoom](nsPresContext* aPc) {
    aPc->SetFullZoom(aFullZoom);
  };
  PropagateToPresContextsHelper(childFn, presContextFn);

  // Dispatch FullZoomChange event only if fullzoom value really has changed.
  if (fullZoomChange) {
    nsContentUtils::DispatchChromeEvent(mDocument, ToSupports(mDocument),
                                        NS_LITERAL_STRING("FullZoomChange"),
                                        CanBubble::eYes, Cancelable::eYes);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetFullZoom(float* aFullZoom) {
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
nsDocumentViewer::GetDeviceFullZoom(float* aDeviceFullZoom) {
  NS_ENSURE_ARG_POINTER(aDeviceFullZoom);
#ifdef NS_PRINT_PREVIEW
  if (GetIsPrintPreview()) {
    // Print Preview overrides all zoom; if specified, we use the print preview
    // zoom, no matter what.
    *aDeviceFullZoom = mPrintPreviewZoom;
    return NS_OK;
  }
#endif
  // If not in print preview, ask the prescontext for the device zoom, if a
  // prescontext is available.
  nsPresContext* pc = GetPresContext();
  *aDeviceFullZoom = pc ? pc->GetDeviceFullZoom() : mPageZoom;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetOverrideDPPX(float aDPPX) {
  // If we don't have a document, then we need to bail.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  mOverrideDPPX = aDPPX;

  auto childFn = [aDPPX](nsDocumentViewer* aChild) {
    aChild->SetOverrideDPPX(aDPPX);
  };
  auto presContextFn = [aDPPX](nsPresContext* aPc) {
    aPc->SetOverrideDPPX(aDPPX);
  };
  PropagateToPresContextsHelper(childFn, presContextFn);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetOverrideDPPX(float* aDPPX) {
  NS_ENSURE_ARG_POINTER(aDPPX);

  nsPresContext* pc = GetPresContext();
  *aDPPX = pc ? pc->GetOverrideDPPX() : mOverrideDPPX;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetAuthorStyleDisabled(bool aStyleDisabled) {
  if (mPresShell) {
    mPresShell->SetAuthorStyleDisabled(aStyleDisabled);
  }

  auto children = [aStyleDisabled](nsDocumentViewer* aChild) {
    aChild->SetAuthorStyleDisabled(aStyleDisabled);
  };

  CallChildren(children);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetAuthorStyleDisabled(bool* aStyleDisabled) {
  if (mPresShell) {
    *aStyleDisabled = mPresShell->GetAuthorStyleDisabled();
  } else {
    *aStyleDisabled = false;
  }
  return NS_OK;
}

void nsDocumentViewer::EmulateMediumInternal(nsAtom* aMedia) {
  auto childFn = [&](nsDocumentViewer* aChild) {
    aChild->EmulateMediumInternal(aMedia);
  };
  auto presContextFn = [&](nsPresContext* aPc) { aPc->EmulateMedium(aMedia); };
  PropagateToPresContextsHelper(childFn, presContextFn);
}

NS_IMETHODIMP
nsDocumentViewer::EmulateMedium(const nsAString& aMediaType) {
  nsAutoString mediaType;
  nsContentUtils::ASCIIToLower(aMediaType, mediaType);
  RefPtr<nsAtom> media = NS_Atomize(mediaType);

  EmulateMediumInternal(media);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::StopEmulatingMedium() {
  EmulateMediumInternal(nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::EmulatePrefersColorScheme(PrefersColorScheme aScheme) {
  auto ToStyle = [](PrefersColorScheme aScheme) -> ColorSchemeOverride {
    switch (aScheme) {
      case PREFERS_COLOR_SCHEME_LIGHT:
        return Some(StylePrefersColorScheme::Light);
      case PREFERS_COLOR_SCHEME_DARK:
        return Some(StylePrefersColorScheme::Dark);
      case PREFERS_COLOR_SCHEME_NO_PREFERENCE:
        return Some(StylePrefersColorScheme::NoPreference);
      case PREFERS_COLOR_SCHEME_NONE:
        return Nothing();
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown prefers color scheme value?");
        return Nothing();
    };
  };

  EmulatePrefersColorSchemeInternal(ToStyle(aScheme));
  return NS_OK;
}

void nsDocumentViewer::EmulatePrefersColorSchemeInternal(
    const ColorSchemeOverride& aOverride) {
  auto childFn = [&aOverride](nsDocumentViewer* aChild) {
    aChild->EmulatePrefersColorSchemeInternal(aOverride);
  };
  auto presContextFn = [&aOverride](nsPresContext* aPc) {
    aPc->SetOverridePrefersColorScheme(aOverride);
  };
  PropagateToPresContextsHelper(childFn, presContextFn);
}

NS_IMETHODIMP nsDocumentViewer::GetForceCharacterSet(
    nsACString& aForceCharacterSet) {
  auto encoding = nsDocumentViewer::GetForceCharset();
  if (encoding) {
    encoding->Name(aForceCharacterSet);
  } else {
    aForceCharacterSet.Truncate();
  }
  return NS_OK;
}

/* [noscript,notxpcom] Encoding getForceCharset (); */
NS_IMETHODIMP_(const Encoding*)
nsDocumentViewer::GetForceCharset() { return mForceCharacterSet; }

NS_IMETHODIMP
nsDocumentViewer::SetForceCharacterSet(const nsACString& aForceCharacterSet) {
  // The empty string means no hint.
  const Encoding* encoding = nullptr;
  if (!aForceCharacterSet.IsEmpty()) {
    if (!(encoding = Encoding::ForLabel(aForceCharacterSet))) {
      // Reject unknown labels
      return NS_ERROR_INVALID_ARG;
    }
  }
  nsDocumentViewer::SetForceCharset(encoding);
  return NS_OK;
}

/* [noscript,notxpcom] void setForceCharset (in Encoding aEncoding); */
NS_IMETHODIMP_(void)
nsDocumentViewer::SetForceCharset(const Encoding* aEncoding) {
  mForceCharacterSet = aEncoding;
  auto childFn = [aEncoding](nsDocumentViewer* aChild) {
    aChild->SetForceCharset(aEncoding);
  };
  // now set the force char set on all children of mContainer
  CallChildren(childFn);
}

NS_IMETHODIMP nsDocumentViewer::GetHintCharacterSet(
    nsACString& aHintCharacterSet) {
  auto encoding = nsDocumentViewer::GetHintCharset();
  if (encoding) {
    encoding->Name(aHintCharacterSet);
  } else {
    aHintCharacterSet.Truncate();
  }
  return NS_OK;
}

/* [noscript,notxpcom] Encoding getHintCharset (); */
NS_IMETHODIMP_(const Encoding*)
nsDocumentViewer::GetHintCharset() {
  if (kCharsetUninitialized == mHintCharsetSource) {
    return nullptr;
  }
  // this can't possibly be right.  we can't set a value just because somebody
  // got a related value!
  // mHintCharsetSource = kCharsetUninitialized;
  return mHintCharset;
}

NS_IMETHODIMP nsDocumentViewer::GetHintCharacterSetSource(
    int32_t* aHintCharacterSetSource) {
  NS_ENSURE_ARG_POINTER(aHintCharacterSetSource);
  *aHintCharacterSetSource = mHintCharsetSource;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetHintCharacterSetSource(int32_t aHintCharacterSetSource) {
  mHintCharsetSource = aHintCharacterSetSource;
  auto childFn = [aHintCharacterSetSource](nsDocumentViewer* aChild) {
    aChild->SetHintCharacterSetSource(aHintCharacterSetSource);
  };
  // now set the force char set on all children of mContainer
  CallChildren(childFn);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetHintCharacterSet(const nsACString& aHintCharacterSet) {
  // The empty string means no hint.
  const Encoding* encoding = nullptr;
  if (!aHintCharacterSet.IsEmpty()) {
    if (!(encoding = Encoding::ForLabel(aHintCharacterSet))) {
      // Reject unknown labels
      return NS_ERROR_INVALID_ARG;
    }
  }
  nsDocumentViewer::SetHintCharset(encoding);
  return NS_OK;
}

/* [noscript,notxpcom] void setHintCharset (in Encoding aEncoding); */
NS_IMETHODIMP_(void)
nsDocumentViewer::SetHintCharset(const Encoding* aEncoding) {
  mHintCharset = aEncoding;
  auto childFn = [aEncoding](nsDocumentViewer* aChild) {
    aChild->SetHintCharset(aEncoding);
  };
  // now set the force char set on all children of mContainer
  CallChildren(childFn);
}

NS_IMETHODIMP nsDocumentViewer::AppendSubtree(
    nsTArray<nsCOMPtr<nsIContentViewer>>& aArray) {
  aArray.AppendElement(this);
  auto childFn = [&aArray](nsDocumentViewer* aChild) {
    aChild->AppendSubtree(aArray);
  };
  CallChildren(childFn);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::PausePainting() {
  CallChildren([](nsDocumentViewer* aChild) { aChild->PausePainting(); });

  if (PresShell* presShell = GetPresShell()) {
    presShell->PausePainting();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::ResumePainting() {
  CallChildren([](nsDocumentViewer* aChild) { aChild->ResumePainting(); });

  if (PresShell* presShell = GetPresShell()) {
    presShell->ResumePainting();
  }

  return NS_OK;
}

nsresult nsDocumentViewer::GetContentSizeInternal(int32_t* aWidth,
                                                  int32_t* aHeight,
                                                  nscoord aMaxWidth,
                                                  nscoord aMaxHeight) {
  NS_ENSURE_TRUE(mDocument, NS_ERROR_NOT_AVAILABLE);

  RefPtr<PresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  // Flush out all content and style updates. We can't use a resize reflow
  // because it won't change some sizes that a style change reflow will.
  mDocument->FlushPendingNotifications(FlushType::Layout);

  nsIFrame* root = presShell->GetRootFrame();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  WritingMode wm = root->GetWritingMode();

  nscoord prefISize;
  {
    RefPtr<gfxContext> rcx(presShell->CreateReferenceRenderingContext());
    nscoord maxISize = wm.IsVertical() ? aMaxHeight : aMaxWidth;
    prefISize = std::min(root->GetPrefISize(rcx), maxISize);
  }

  // We should never intentionally get here with this sentinel value, but it's
  // possible that a document with huge sizes might inadvertently have a
  // prefISize that exactly matches NS_UNCONSTRAINEDSIZE.
  // Just bail if that happens.
  NS_ENSURE_TRUE(prefISize != NS_UNCONSTRAINEDSIZE, NS_ERROR_FAILURE);

  nscoord height = wm.IsVertical() ? prefISize : aMaxHeight;
  nscoord width = wm.IsVertical() ? aMaxWidth : prefISize;
  nsresult rv =
      presShell->ResizeReflow(width, height, ResizeReflowOptions::BSizeLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsPresContext> presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  // Protect against bogus returns here
  nsRect shellArea = presContext->GetVisibleArea();
  NS_ENSURE_TRUE(shellArea.width != NS_UNCONSTRAINEDSIZE &&
                     shellArea.height != NS_UNCONSTRAINEDSIZE,
                 NS_ERROR_FAILURE);

  // Ceil instead of rounding here, so we can actually guarantee showing all the
  // content.
  *aWidth = std::ceil(presContext->AppUnitsToFloatDevPixels(shellArea.width));
  *aHeight = std::ceil(presContext->AppUnitsToFloatDevPixels(shellArea.height));

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetContentSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_TRUE(mContainer, NS_ERROR_NOT_AVAILABLE);

  RefPtr<BrowsingContext> bc = mContainer->GetBrowsingContext();
  NS_ENSURE_TRUE(bc, NS_ERROR_NOT_AVAILABLE);

  // It's only valid to access this from a top frame.  Doesn't work from
  // sub-frames.
  NS_ENSURE_TRUE(bc->IsTop(), NS_ERROR_FAILURE);

  return GetContentSizeInternal(aWidth, aHeight, NS_UNCONSTRAINEDSIZE,
                                NS_UNCONSTRAINEDSIZE);
}

NS_IMETHODIMP
nsDocumentViewer::GetContentSizeConstrained(int32_t aMaxWidth,
                                            int32_t aMaxHeight, int32_t* aWidth,
                                            int32_t* aHeight) {
  RefPtr<nsPresContext> presContext = GetPresContext();
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

/*
 * GetPopupNode, GetPopupLinkNode and GetPopupImageNode are helpers
 * for the cmd_copyLink / cmd_copyImageLocation / cmd_copyImageContents family
 * of commands. The focus controller stores the popup node, these retrieve
 * them and munge appropriately. Note that we have to store the popup node
 * rather than retrieving it from EventStateManager::GetFocusedContent because
 * not all content (images included) can receive focus.
 */

already_AddRefed<nsINode> nsDocumentViewer::GetPopupNode() {
  // get the document
  Document* document = GetDocument();
  NS_ENSURE_TRUE(document, nullptr);

  // get the private dom window
  nsCOMPtr<nsPIDOMWindowOuter> window(document->GetWindow());
  NS_ENSURE_TRUE(window, nullptr);
  if (window) {
    nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
    NS_ENSURE_TRUE(root, nullptr);

    // get the popup node
    nsCOMPtr<nsINode> node = root->GetPopupNode();
#ifdef MOZ_XUL
    if (!node) {
      nsPIDOMWindowOuter* rootWindow = root->GetWindow();
      if (rootWindow) {
        nsCOMPtr<Document> rootDoc = rootWindow->GetExtantDoc();
        if (rootDoc) {
          nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
          if (pm) {
            node = pm->GetLastTriggerPopupNode(rootDoc);
          }
        }
      }
    }
#endif
    return node.forget();
  }

  return nullptr;
}

// GetPopupLinkNode: return popup link node or fail
already_AddRefed<nsINode> nsDocumentViewer::GetPopupLinkNode() {
  // find popup node
  nsCOMPtr<nsINode> node = GetPopupNode();

  // find out if we have a link in our ancestry
  while (node) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(node));
    if (content) {
      nsCOMPtr<nsIURI> hrefURI = content->GetHrefURI();
      if (hrefURI) {
        return node.forget();
      }
    }

    // get our parent and keep trying...
    node = node->GetParentNode();
  }

  // if we have no node, fail
  return nullptr;
}

// GetPopupLinkNode: return popup image node or fail
already_AddRefed<nsIImageLoadingContent> nsDocumentViewer::GetPopupImageNode() {
  // find popup node
  nsCOMPtr<nsINode> node = GetPopupNode();
  nsCOMPtr<nsIImageLoadingContent> img = do_QueryInterface(node);
  return img.forget();
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

NS_IMETHODIMP nsDocumentViewer::GetInLink(bool* aInLink) {
#ifdef DEBUG_dr
  printf("dr :: nsDocumentViewer::GetInLink\n");
#endif

  NS_ENSURE_ARG_POINTER(aInLink);

  // we're not in a link unless i say so
  *aInLink = false;

  // get the popup link
  nsCOMPtr<nsINode> node = GetPopupLinkNode();
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  // if we made it here, we're in a link
  *aInLink = true;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentViewer::GetInImage(bool* aInImage) {
#ifdef DEBUG_dr
  printf("dr :: nsDocumentViewer::GetInImage\n");
#endif

  NS_ENSURE_ARG_POINTER(aInImage);

  // we're not in an image unless i say so
  *aInImage = false;

  // get the popup image
  nsCOMPtr<nsIImageLoadingContent> node = GetPopupImageNode();
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

NS_IMETHODIMP nsDocViewerSelectionListener::NotifySelectionChanged(
    Document*, Selection*, int16_t aReason) {
  if (!mDocViewer) {
    return NS_OK;
  }

  // get the selection state
  RefPtr<mozilla::dom::Selection> selection =
      mDocViewer->GetDocumentSelection();
  if (!selection) {
    return NS_ERROR_FAILURE;
  }

  Document* theDoc = mDocViewer->GetDocument();
  if (!theDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIDOMWindowOuter> domWindow = theDoc->GetWindow();
  if (!domWindow) return NS_ERROR_FAILURE;

  bool selectionCollapsed = selection->IsCollapsed();
  // We only call UpdateCommands when the selection changes from collapsed to
  // non-collapsed or vice versa, however we skip the initializing collapse. We
  // might need another update string for simple selection changes, but that
  // would be expenseive.
  if (mSelectionWasCollapsed != selectionCollapsed) {
    domWindow->UpdateCommands(NS_LITERAL_STRING("select"), selection, aReason);
    mSelectionWasCollapsed = selectionCollapsed;
  }

  return NS_OK;
}

// nsDocViewerFocusListener
NS_IMPL_ISUPPORTS(nsDocViewerFocusListener, nsIDOMEventListener)

nsresult nsDocViewerFocusListener::HandleEvent(Event* aEvent) {
  NS_ENSURE_STATE(mDocViewer);

  RefPtr<PresShell> presShell = mDocViewer->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  RefPtr<nsFrameSelection> selection =
      presShell->GetLastFocusedFrameSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  auto selectionStatus = selection->GetDisplaySelection();
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("focus")) {
    // If selection was disabled, re-enable it.
    if (selectionStatus == nsISelectionController::SELECTION_DISABLED ||
        selectionStatus == nsISelectionController::SELECTION_HIDDEN) {
      selection->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      selection->RepaintSelection(SelectionType::eNormal);
    }
  } else {
    MOZ_ASSERT(eventType.EqualsLiteral("blur"), "Unexpected event type");
    // If selection was on, disable it.
    if (selectionStatus == nsISelectionController::SELECTION_ON ||
        selectionStatus == nsISelectionController::SELECTION_ATTENTION) {
      selection->SetDisplaySelection(
          nsISelectionController::SELECTION_DISABLED);
      selection->RepaintSelection(SelectionType::eNormal);
    }
  }

  return NS_OK;
}

/** ---------------------------------------------------
 *  From nsIWebBrowserPrint
 */

#ifdef NS_PRINTING

NS_IMETHODIMP
nsDocumentViewer::Print(nsIPrintSettings* aPrintSettings,
                        nsIWebProgressListener* aWebProgressListener) {
  if (!mContainer) {
    PR_PL(("Container was destroyed yet we are still trying to use it!"));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  NS_ENSURE_STATE(docShell);

  // Check to see if this document is still busy
  // If it is busy and we aren't already "queued" up to print then
  // Indicate there is a print pending and cache the args for later
  auto busyFlags = docShell->GetBusyFlags();
  if (busyFlags != nsIDocShell::BUSY_FLAGS_NONE &&
      busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING &&
      !mPrintDocIsFullyLoaded) {
    if (!mPrintIsPending) {
      mCachedPrintSettings = aPrintSettings;
      mCachedPrintWebProgressListner = aWebProgressListener;
      mPrintIsPending = true;
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
  // the only time we can print more than one job at a time is the regression
  // tests
  if (GetIsPrinting()) {
    // Let the user know we are not ready to print.
    rv = NS_ERROR_NOT_AVAILABLE;

    if (mPrintJob) {
      mPrintJob->FirePrintingErrorEvent(rv);
    }

    return rv;
  }

  // Dispatch 'beforeprint' event and ensure 'afterprint' will be dispatched:
  MOZ_ASSERT(!mAutoBeforeAndAfterPrint,
             "We don't want to dispatch nested beforeprint/afterprint");
  auto autoBeforeAndAfterPrint =
      MakeUnique<AutoPrintEventDispatcher>(mDocument);
  NS_ENSURE_STATE(!GetIsPrinting());
  // If we are hosting a full-page plugin, tell it to print
  // first. It shows its own native print UI.
  nsCOMPtr<nsIPluginDocument> pDoc(do_QueryInterface(mDocument));
  if (pDoc) return pDoc->Print();

  // Our call to nsPrintJob::Print() may cause mPrintJob to be
  // Release()'d in Destroy().  Therefore, we need to grab the instance with
  // a local variable, so that it won't be deleted during its own method.
  RefPtr<nsPrintJob> printJob = mPrintJob;
  if (!printJob) {
    NS_ENSURE_STATE(mDeviceContext);
    printJob = new nsPrintJob();

    rv = printJob->Initialize(this, mContainer, mDocument,
                              float(AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom);
    if (NS_FAILED(rv)) {
      printJob->Destroy();
      return rv;
    }
    mPrintJob = printJob;
  }
  if (printJob->HasPrintCallbackCanvas()) {
    // Postpone the 'afterprint' event until after the mozPrintCallback
    // callbacks have been called:
    mAutoBeforeAndAfterPrint = std::move(autoBeforeAndAfterPrint);
  }
  rv = printJob->Print(mDocument, aPrintSettings, aWebProgressListener);
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
}

NS_IMETHODIMP
nsDocumentViewer::PrintPreview(nsIPrintSettings* aPrintSettings,
                               mozIDOMWindowProxy* aChildDOMWin,
                               nsIWebProgressListener* aWebProgressListener) {
#  ifdef NS_PRINT_PREVIEW
  MOZ_ASSERT(IsInitializedForPrintPreview(),
             "For print preview nsIWebBrowserPrint must be from "
             "docshell.initOrReusePrintPreviewViewer!");

  NS_ENSURE_ARG_POINTER(aChildDOMWin);
  nsresult rv = NS_OK;

  if (GetIsPrinting()) {
    nsPrintJob::CloseProgressDialog(aWebProgressListener);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  if (!docShell || !mDeviceContext) {
    PR_PL(("Can't Print Preview without device context and docshell"));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aChildDOMWin);
  MOZ_ASSERT(window);
  nsCOMPtr<Document> doc = window->GetDoc();
  NS_ENSURE_STATE(doc);

  // Dispatch 'beforeprint' event and ensure 'afterprint' will be dispatched:
  // XXX Currently[1] when the user switches between portrait and landscape
  // mode in print preview, we re-enter this function before
  // mAutoBeforeAndAfterPrint (if set) is cleared to dispatch the 'afterprint'
  // event.  To avoid sending multiple 'beforeprint'/'afterprint' events we
  // must avoid creating a new AutoPrintEventDispatcher object here if we
  // already have one saved in mAutoBeforeAndAfterPrint.
  // [1] Until PDF.js is removed (though, maybe after that as well).
  UniquePtr<AutoPrintEventDispatcher> autoBeforeAndAfterPrint;
  if (!mAutoBeforeAndAfterPrint) {
    autoBeforeAndAfterPrint = MakeUnique<AutoPrintEventDispatcher>(doc);
  }
  NS_ENSURE_STATE(!GetIsPrinting());
  // beforeprint event may have caused ContentViewer to be shutdown.
  NS_ENSURE_STATE(mContainer);
  NS_ENSURE_STATE(mDeviceContext);

  // Our call to nsPrintJob::PrintPreview() may cause mPrintJob to be
  // Release()'d in Destroy().  Therefore, we need to grab the instance with
  // a local variable, so that it won't be deleted during its own method.
  RefPtr<nsPrintJob> printJob = mPrintJob;
  if (!printJob) {
    printJob = new nsPrintJob();

    rv = printJob->Initialize(this, mContainer, doc,
                              float(AppUnitsPerCSSInch()) /
                                  float(mDeviceContext->AppUnitsPerDevPixel()) /
                                  mPageZoom);
    if (NS_FAILED(rv)) {
      printJob->Destroy();
      return rv;
    }
    mPrintJob = printJob;
  }
  if (autoBeforeAndAfterPrint && printJob->HasPrintCallbackCanvas()) {
    // Postpone the 'afterprint' event until after the mozPrintCallback
    // callbacks have been called:
    mAutoBeforeAndAfterPrint = std::move(autoBeforeAndAfterPrint);
  }
  rv = printJob->PrintPreview(doc, aPrintSettings, aWebProgressListener);
  mPrintPreviewZoomed = false;
  if (NS_FAILED(rv)) {
    OnDonePrinting();
  }
  return rv;
#  else
  return NS_ERROR_FAILURE;
#  endif  // NS_PRINT_PREVIEW
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsDocumentViewer::PrintPreviewScrollToPage(int16_t aType, int32_t aPageNum) {
  if (!GetIsPrintPreview() || mPrintJob->GetIsCreatingPrintPreview())
    return NS_ERROR_FAILURE;

  nsIScrollableFrame* sf =
      mPrintJob->GetPrintPreviewPresShell()->GetRootScrollFrameAsScrollable();
  if (!sf) return NS_OK;

  // Check to see if we can short circut scrolling to the top
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_HOME ||
      (aType == nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM &&
       aPageNum == 1)) {
    sf->ScrollTo(nsPoint(0, 0), ScrollMode::Instant);
    return NS_OK;
  }

  // in PP mPrtPreview->mPrintObject->mSeqFrame is null
  nsIFrame* seqFrame = nullptr;
  int32_t pageCount = 0;
  if (NS_FAILED(mPrintJob->GetSeqFrameAndCountPages(seqFrame, pageCount))) {
    return NS_ERROR_FAILURE;
  }

  // Figure where we are currently scrolled to
  nsPoint pt = sf->GetScrollPosition();

  int32_t pageNum = 1;
  nsIFrame* fndPageFrame = nullptr;
  nsIFrame* currentPage = nullptr;

  // If it is "End" then just do a "goto" to the last page
  if (aType == nsIWebBrowserPrint::PRINTPREVIEW_END) {
    aType = nsIWebBrowserPrint::PRINTPREVIEW_GOTO_PAGENUM;
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
  } else {  // If we get here we are doing "GoTo"
    if (aPageNum < 0 || aPageNum > pageCount) {
      return NS_OK;
    }
  }

  if (fndPageFrame) {
    nscoord newYPosn = nscoord(mPrintJob->GetPrintPreviewScale() *
                               fndPageFrame->GetPosition().y);
    sf->ScrollTo(nsPoint(pt.x, newYPosn), ScrollMode::Instant);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetGlobalPrintSettings(
    nsIPrintSettings** aGlobalPrintSettings) {
  return nsPrintJob::GetGlobalPrintSettings(aGlobalPrintSettings);
}

// XXX This always returns false for subdocuments
NS_IMETHODIMP
nsDocumentViewer::GetDoingPrint(bool* aDoingPrint) {
  NS_ENSURE_ARG_POINTER(aDoingPrint);

  // XXX shouldn't this be GetDoingPrint() ?
  *aDoingPrint = mPrintJob ? mPrintJob->IsDoingPrintPreview() : false;
  return NS_OK;
}

// XXX This always returns false for subdocuments
NS_IMETHODIMP
nsDocumentViewer::GetDoingPrintPreview(bool* aDoingPrintPreview) {
  NS_ENSURE_ARG_POINTER(aDoingPrintPreview);

  *aDoingPrintPreview = mPrintJob ? mPrintJob->IsDoingPrintPreview() : false;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetCurrentPrintSettings(
    nsIPrintSettings** aCurrentPrintSettings) {
  NS_ENSURE_ARG_POINTER(aCurrentPrintSettings);

  *aCurrentPrintSettings = nullptr;
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);

  *aCurrentPrintSettings = mPrintJob->GetCurrentPrintSettings().take();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::Cancel() {
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);
  return mPrintJob->Cancel();
}

#  ifdef NS_PRINT_PREVIEW
// Reset ESM focus for all descendent doc shells.
static void ResetFocusState(nsIDocShell* aDocShell) {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  nsTArray<RefPtr<nsIDocShell>> docShells;
  aDocShell->GetAllDocShellsInSubtree(nsIDocShellTreeItem::typeContent,
                                      nsIDocShell::ENUMERATE_FORWARDS,
                                      docShells);

  for (const auto& currentContainer : docShells) {
    nsCOMPtr<nsPIDOMWindowOuter> win = do_GetInterface(currentContainer);
    if (win) {
      fm->ClearFocus(win);
    }
  }
}
#  endif  // NS_PRINT_PREVIEW

NS_IMETHODIMP
nsDocumentViewer::ExitPrintPreview() {
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);

  if (GetIsPrinting()) {
    // Block exiting the print preview window if we're in the middle of an
    // actual print.
    return NS_ERROR_FAILURE;
  }

  if (!GetIsPrintPreview()) {
    NS_ERROR("Wow, we should never get here!");
    return NS_OK;
  }

#  ifdef NS_PRINT_PREVIEW
  mPrintJob->TurnScriptingOn(true);
  mPrintJob->Destroy();
  mPrintJob = nullptr;

  // Nowadays we use a static clone document for printing, and print preview is
  // in a separate tab that gets closed after print preview finishes.  Probably
  // nothing below this line is necessary anymore.

  SetIsPrintPreview(false);

  nsCOMPtr<nsIDocShell> docShell(mContainer);
  ResetFocusState(docShell);

  SetTextZoom(mTextZoom);
  SetFullZoom(mPageZoom);
  SetOverrideDPPX(mOverrideDPPX);
  Show();
#  endif  // NS_PRINT_PREVIEW

  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetPrintPreviewNumPages(int32_t* aPrintPreviewNumPages) {
  NS_ENSURE_ARG_POINTER(aPrintPreviewNumPages);
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);

  *aPrintPreviewNumPages = mPrintJob->GetPrintPreviewNumPages();
  return *aPrintPreviewNumPages > 0 ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsIFrameSelected(bool* aIsIFrameSelected) {
  *aIsIFrameSelected = false;
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);

  *aIsIFrameSelected = mPrintJob->IsIFrameSelected();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsRangeSelection(bool* aIsRangeSelection) {
  *aIsRangeSelection = false;
  NS_ENSURE_TRUE(mPrintJob, NS_ERROR_FAILURE);

  *aIsRangeSelection = mPrintJob->IsRangeSelection();
  return NS_OK;
}

//----------------------------------------------------------------------------------
// Printing/Print Preview Helpers
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Walks the document tree and tells each DocShell whether Printing/PP is
// happening
void nsDocumentViewer::SetIsPrintingInDocShellTree(
    nsIDocShellTreeItem* aParentNode, bool aIsPrintingOrPP, bool aStartAtTop) {
  nsCOMPtr<nsIDocShellTreeItem> parentItem(aParentNode);

  // find top of "same parent" tree
  if (aStartAtTop) {
    if (aIsPrintingOrPP) {
      while (parentItem) {
        nsCOMPtr<nsIDocShellTreeItem> parent;
        parentItem->GetInProcessSameTypeParent(getter_AddRefs(parent));
        if (!parent) {
          break;
        }
        parentItem = parent;
      }
      mTopContainerWhilePrinting = do_GetWeakReference(parentItem);
    } else {
      parentItem = do_QueryReferent(mTopContainerWhilePrinting);
    }
  }

  // Check to see if the DocShell's ContentViewer is printing/PP
  nsCOMPtr<nsIDocShell> viewerContainer = do_QueryInterface(parentItem);
  if (viewerContainer) {
    viewerContainer->SetIsPrinting(aIsPrintingOrPP);
  }

  if (!aParentNode) {
    return;
  }

  // Traverse children to see if any of them are printing.
  int32_t n;
  aParentNode->GetInProcessChildCount(&n);
  for (int32_t i = 0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aParentNode->GetInProcessChildAt(i, getter_AddRefs(child));
    NS_ASSERTION(child, "child isn't nsIDocShell");
    if (child) {
      SetIsPrintingInDocShellTree(child, aIsPrintingOrPP, false);
    }
  }
}
#endif  // NS_PRINTING

bool nsDocumentViewer::ShouldAttachToTopLevel() {
  if (!mParentWidget) {
    return false;
  }

  if (!mContainer) {
    return false;
  }

  // We always attach when using puppet widgets
  if (nsIWidget::UsePuppetWidgets()) {
    return true;
  }

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  if (!mPresContext) {
    return false;
  }

  // On windows, in the parent process we also attach, but just to
  // chrome items
  nsWindowType winType = mParentWidget->WindowType();
  if ((winType == eWindowType_toplevel || winType == eWindowType_dialog ||
       winType == eWindowType_invisible) &&
      mPresContext->IsChrome()) {
    return true;
  }
#endif

  return false;
}

//------------------------------------------------------------
// XXX this always returns false for subdocuments
bool nsDocumentViewer::GetIsPrinting() {
#ifdef NS_PRINTING
  if (mPrintJob) {
    return mPrintJob->GetIsPrinting();
  }
#endif
  return false;
}

//------------------------------------------------------------
// Notification from the PrintJob of the current Printing status
void nsDocumentViewer::SetIsPrinting(bool aIsPrinting) {
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
// The PrintJob holds the current value
// this called from inside the DocViewer.
// XXX it always returns false for subdocuments
bool nsDocumentViewer::GetIsPrintPreview() {
#ifdef NS_PRINTING
  if (mPrintJob) {
    return mPrintJob->GetIsPrintPreview();
  }
#endif
  return false;
}

//------------------------------------------------------------
// Notification from the PrintJob of the current PP status
void nsDocumentViewer::SetIsPrintPreview(bool aIsPrintPreview) {
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

  // Protect against pres shell destruction running scripts.
  nsAutoScriptBlocker scriptBlocker;

  if (!aIsPrintPreview) {
    InvalidatePotentialSubDocDisplayItem();
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
void nsDocumentViewer::IncrementDestroyBlockedCount() {
  ++mDestroyBlockedCount;
}

void nsDocumentViewer::DecrementDestroyBlockedCount() {
  --mDestroyBlockedCount;
}

//------------------------------------------------------------
// This called ONLY when printing has completed and the DV
// is being notified that it should get rid of the nsPrintJob.
//
// BUT, if we are in Print Preview then we want to ignore the
// notification (we do not get rid of the nsPrintJob)
//
// One small caveat:
//   This IS called from two places in this module for cleaning
//   up when an error occurred during the start up printing
//   and print preview
//
void nsDocumentViewer::OnDonePrinting() {
#if defined(NS_PRINTING) && defined(NS_PRINT_PREVIEW)
  // If Destroy() has been called during calling nsPrintJob::Print() or
  // nsPrintJob::PrintPreview(), mPrintJob is already nullptr here.
  // So, the following clean up does nothing in such case.
  // (Do we need some of this for that case?)
  if (mPrintJob) {
    RefPtr<nsPrintJob> printJob = mPrintJob;
    if (GetIsPrintPreview()) {
      printJob->DestroyPrintingData();
    } else {
      mPrintJob = nullptr;
      printJob->Destroy();
    }

    // We are done printing, now cleanup
    if (mDeferredWindowClose) {
      mDeferredWindowClose = false;
      if (mContainer) {
        if (nsCOMPtr<nsPIDOMWindowOuter> win = mContainer->GetWindow()) {
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
#endif  // NS_PRINTING && NS_PRINT_PREVIEW
}

NS_IMETHODIMP nsDocumentViewer::SetPageModeForTesting(
    bool aPageMode, nsIPrintSettings* aPrintSettings) {
  // XXX Page mode is only partially working; it's currently used for
  // reftests that require a paginated context
  mIsPageMode = aPageMode;

  // The DestroyPresShell call requires a script blocker, since the
  // PresShell::Destroy call it does can cause scripts to run, which could
  // re-entrantly call methods on the nsDocumentViewer.
  nsAutoScriptBlocker scriptBlocker;

  if (mPresShell) {
    DestroyPresShell();
  }

  if (mPresContext) {
    DestroyPresContext();
  }

  mViewManager = nullptr;
  mWindow = nullptr;

  NS_ENSURE_STATE(mDocument);
  if (aPageMode) {
    mPresContext = CreatePresContext(
        mDocument, nsPresContext::eContext_PageLayout, FindContainerView());
    NS_ENSURE_TRUE(mPresContext, NS_ERROR_OUT_OF_MEMORY);
    mPresContext->SetPaginatedScrolling(true);
    mPresContext->SetPrintSettings(aPrintSettings);
    nsresult rv = mPresContext->Init(mDeviceContext);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(InitInternal(mParentWidget, nullptr, nullptr, mBounds, true,
                                 false, false),
                    NS_ERROR_FAILURE);

  Show();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetHistoryEntry(nsISHEntry** aHistoryEntry) {
  NS_IF_ADDREF(*aHistoryEntry = mSHEntry);
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsTabModalPromptAllowed(bool* aAllowed) {
  *aAllowed = !mHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::GetIsHidden(bool* aHidden) {
  *aHidden = mHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentViewer::SetIsHidden(bool aHidden) {
  mHidden = aHidden;
  return NS_OK;
}

void nsDocumentViewer::DestroyPresShell() {
  // We assert this because destroying the pres shell could otherwise cause
  // re-entrancy into nsDocumentViewer methods, and all callers of
  // DestroyPresShell need to do other cleanup work afterwards before it
  // is safe for those re-entrant method calls to be made.
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
             "DestroyPresShell must only be called when scripts are blocked");

  // Break circular reference (or something)
  mPresShell->EndObservingDocument();

  RefPtr<mozilla::dom::Selection> selection = GetDocumentSelection();
  if (selection && mSelectionListener)
    selection->RemoveSelectionListener(mSelectionListener);

  mPresShell->Destroy();
  mPresShell = nullptr;
}

void nsDocumentViewer::InvalidatePotentialSubDocDisplayItem() {
  if (mViewManager) {
    if (nsView* rootView = mViewManager->GetRootView()) {
      if (nsView* rootViewParent = rootView->GetParent()) {
        if (nsView* subdocview = rootViewParent->GetParent()) {
          if (nsIFrame* f = subdocview->GetFrame()) {
            if (nsSubDocumentFrame* s = do_QueryFrame(f)) {
              s->MarkNeedsDisplayItemRebuild();
            }
          }
        }
      }
    }
  }
}

void nsDocumentViewer::DestroyPresContext() {
  InvalidatePotentialSubDocDisplayItem();
  mPresContext = nullptr;
}

bool nsDocumentViewer::IsInitializedForPrintPreview() {
  return mInitializedForPrintPreview;
}

void nsDocumentViewer::InitializeForPrintPreview() {
  mInitializedForPrintPreview = true;
}

void nsDocumentViewer::SetPrintPreviewPresentation(nsViewManager* aViewManager,
                                                   nsPresContext* aPresContext,
                                                   PresShell* aPresShell) {
  // Protect against pres shell destruction running scripts and re-entrantly
  // creating a new presentation.
  nsAutoScriptBlocker scriptBlocker;

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
nsDocumentShownDispatcher::Run() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(ToSupports(mDocument), "document-shown",
                                     nullptr);
  }
  return NS_OK;
}

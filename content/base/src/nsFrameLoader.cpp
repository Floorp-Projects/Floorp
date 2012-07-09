/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#include "base/basictypes.h"

#include "prenv.h"

#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMMozBrowserFrame.h"
#include "nsIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsFrameLoader.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsDOMError.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIDocShellHistory.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXULWindow.h"
#include "nsIEditor.h"
#include "nsIEditorDocShell.h"
#include "nsIMozBrowserFrame.h"

#include "nsLayoutUtils.h"
#include "nsIView.h"
#include "nsAsyncDOMEvent.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"

#include "nsThreadUtils.h"

#include "nsIDOMChromeWindow.h"
#include "nsInProcessTabChildGlobal.h"

#include "Layers.h"

#include "ContentParent.h"
#include "TabParent.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/layout/RenderFrameParent.h"

#include "jsapi.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::layout;
typedef FrameMetrics::ViewID ViewID;

class nsAsyncDocShellDestroyer : public nsRunnable
{
public:
  nsAsyncDocShellDestroyer(nsIDocShell* aDocShell)
    : mDocShell(aDocShell)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
    if (base_win) {
      base_win->Destroy();
    }
    return NS_OK;
  }
  nsRefPtr<nsIDocShell> mDocShell;
};

static void InvalidateFrame(nsIFrame* aFrame, PRUint32 aFlags)
{
  if (!aFrame)
    return;
  nsRect rect = nsRect(nsPoint(0, 0), aFrame->GetRect().Size());
  aFrame->InvalidateWithFlags(rect, aFlags);
}

NS_IMPL_ISUPPORTS1(nsContentView, nsIContentView)

bool
nsContentView::IsRoot() const
{
  return mScrollId == FrameMetrics::ROOT_SCROLL_ID;
}

nsresult
nsContentView::Update(const ViewConfig& aConfig)
{
  if (aConfig == mConfig) {
    return NS_OK;
  }
  mConfig = aConfig;

  // View changed.  Try to locate our subdoc frame and invalidate
  // it if found.
  if (!mFrameLoader) {
    if (IsRoot()) {
      // Oops, don't have a frame right now.  That's OK; the view
      // config persists and will apply to the next frame we get, if we
      // ever get one.
      return NS_OK;
    } else {
      // This view is no longer valid.
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (RenderFrameParent* rfp = mFrameLoader->GetCurrentRemoteFrame()) {
    rfp->ContentViewScaleChanged(this);
  }

  // XXX could be clever here and compute a smaller invalidation
  // rect
  // NB: we pass INVALIDATE_NO_THEBES_LAYERS here to keep view
  // semantics the same for both in-process and out-of-process
  // <browser>.  This is just a transform of the layer subtree in
  // both.
  InvalidateFrame(mFrameLoader->GetPrimaryFrameOfOwningContent(), nsIFrame::INVALIDATE_NO_THEBES_LAYERS);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::ScrollTo(float aXpx, float aYpx)
{
  ViewConfig config(mConfig);
  config.mScrollOffset = nsPoint(nsPresContext::CSSPixelsToAppUnits(aXpx),
                                 nsPresContext::CSSPixelsToAppUnits(aYpx));
  return Update(config);
}

NS_IMETHODIMP
nsContentView::ScrollBy(float aDXpx, float aDYpx)
{
  ViewConfig config(mConfig);
  config.mScrollOffset.MoveBy(nsPresContext::CSSPixelsToAppUnits(aDXpx),
                              nsPresContext::CSSPixelsToAppUnits(aDYpx));
  return Update(config);
}

NS_IMETHODIMP
nsContentView::SetScale(float aXScale, float aYScale)
{
  ViewConfig config(mConfig);
  config.mXScale = aXScale;
  config.mYScale = aYScale;
  return Update(config);
}

NS_IMETHODIMP
nsContentView::GetScrollX(float* aViewScrollX)
{
  *aViewScrollX = nsPresContext::AppUnitsToFloatCSSPixels(
    mConfig.mScrollOffset.x);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetScrollY(float* aViewScrollY)
{
  *aViewScrollY = nsPresContext::AppUnitsToFloatCSSPixels(
    mConfig.mScrollOffset.y);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetViewportWidth(float* aWidth)
{
  *aWidth = nsPresContext::AppUnitsToFloatCSSPixels(mViewportSize.width);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetViewportHeight(float* aHeight)
{
  *aHeight = nsPresContext::AppUnitsToFloatCSSPixels(mViewportSize.height);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetContentWidth(float* aWidth)
{
  *aWidth = nsPresContext::AppUnitsToFloatCSSPixels(mContentSize.width);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetContentHeight(float* aHeight)
{
  *aHeight = nsPresContext::AppUnitsToFloatCSSPixels(mContentSize.height);
  return NS_OK;
}

NS_IMETHODIMP
nsContentView::GetId(nsContentViewId* aId)
{
  NS_ASSERTION(sizeof(nsContentViewId) == sizeof(ViewID),
               "ID size for XPCOM ID and internal ID type are not the same!");
  *aId = mScrollId;
  return NS_OK;
}

// Bug 136580: Limit to the number of nested content frames that can have the
//             same URL. This is to stop content that is recursively loading
//             itself.  Note that "#foo" on the end of URL doesn't affect
//             whether it's considered identical, but "?foo" or ";foo" are
//             considered and compared.
// Bug 228829: Limit this to 1, like IE does.
#define MAX_SAME_URL_CONTENT_FRAMES 1

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameLoader)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocShell)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsFrameLoader::mMessageManager");
  cb.NoteXPCOMChild(static_cast<nsIContentFrameMessageManager*>(tmp->mMessageManager.get()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIContentViewManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFrameLoader)
NS_INTERFACE_MAP_END

nsFrameLoader::nsFrameLoader(Element* aOwner, bool aNetworkCreated)
  : mOwnerContent(aOwner)
  , mDepthTooGreat(false)
  , mIsTopLevelContent(false)
  , mDestroyCalled(false)
  , mNeedsAsyncDestroy(false)
  , mInSwap(false)
  , mInShow(false)
  , mHideCalled(false)
  , mNetworkCreated(aNetworkCreated)
  , mDelayRemoteDialogs(false)
  , mRemoteBrowserShown(false)
  , mRemoteFrame(false)
  , mClipSubdocument(true)
  , mClampScrollPosition(true)
  , mCurrentRemoteFrame(nsnull)
  , mRemoteBrowser(nsnull)
  , mRenderMode(RENDER_MODE_DEFAULT)
  , mEventMode(EVENT_MODE_NORMAL_DISPATCH)
{
}

nsFrameLoader*
nsFrameLoader::Create(Element* aOwner, bool aNetworkCreated)
{
  NS_ENSURE_TRUE(aOwner, nsnull);
  nsIDocument* doc = aOwner->OwnerDoc();
  NS_ENSURE_TRUE(!doc->GetDisplayDocument() &&
                 ((!doc->IsLoadedAsData() && aOwner->GetCurrentDoc()) ||
                   doc->IsStaticDocument()),
                 nsnull);

  return new nsFrameLoader(aOwner, aNetworkCreated);
}

NS_IMETHODIMP
nsFrameLoader::LoadFrame()
{
  NS_ENSURE_TRUE(mOwnerContent, NS_ERROR_NOT_INITIALIZED);

  nsAutoString src;
  GetURL(src);

  src.Trim(" \t\n\r");

  if (src.IsEmpty()) {
    src.AssignLiteral("about:blank");
  }

  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (doc->IsStaticDocument()) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> base_uri = mOwnerContent->GetBaseURI();
  const nsAFlatCString &doc_charset = doc->GetDocumentCharacterSet();
  const char *charset = doc_charset.IsEmpty() ? nsnull : doc_charset.get();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), src, charset, base_uri);

  // If the URI was malformed, try to recover by loading about:blank.
  if (rv == NS_ERROR_MALFORMED_URI) {
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("about:blank"),
                   charset, base_uri);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = LoadURI(uri);
  }
  
  if (NS_FAILED(rv)) {
    FireErrorEvent();

    return rv;
  }

  return NS_OK;
}

void
nsFrameLoader::FireErrorEvent()
{
  if (mOwnerContent) {
    nsRefPtr<nsAsyncDOMEvent> event =
      new nsLoadBlockingAsyncDOMEvent(mOwnerContent, NS_LITERAL_STRING("error"),
                                      false, false);
    event->PostDOMEvent();
  }
}

NS_IMETHODIMP
nsFrameLoader::LoadURI(nsIURI* aURI)
{
  if (!aURI)
    return NS_ERROR_INVALID_POINTER;
  NS_ENSURE_STATE(!mDestroyCalled && mOwnerContent);

  nsCOMPtr<nsIDocument> doc = mOwnerContent->OwnerDoc();

  nsresult rv = CheckURILoad(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mURIToLoad = aURI;
  rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mURIToLoad = nsnull;
  }
  return rv;
}

nsresult
nsFrameLoader::ReallyStartLoading()
{
  nsresult rv = ReallyStartLoadingInternal();
  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }
  
  return rv;
}

nsresult
nsFrameLoader::ReallyStartLoadingInternal()
{
  NS_ENSURE_STATE(mURIToLoad && mOwnerContent && mOwnerContent->IsInDoc());

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mRemoteFrame) {
    if (!mRemoteBrowser) {
      TryRemoteBrowser();

      if (!mRemoteBrowser) {
        NS_WARNING("Couldn't create child process for iframe.");
        return NS_ERROR_FAILURE;
      }
    }

    if (mRemoteBrowserShown || ShowRemoteFrame(nsIntSize(0, 0))) {
      // FIXME get error codes from child
      mRemoteBrowser->LoadURL(mURIToLoad);
    } else {
      NS_WARNING("[nsFrameLoader] ReallyStartLoadingInternal tried but couldn't show remote browser.\n");
    }

    return NS_OK;
  }

  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded with a null mDocShell");

  // Just to be safe, recheck uri.
  rv = CheckURILoad(mURIToLoad);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!
  loadInfo->SetOwner(mOwnerContent->NodePrincipal());

  nsCOMPtr<nsIURI> referrer;
  rv = mOwnerContent->NodePrincipal()->GetURI(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo->SetReferrer(referrer);

  // Kick off the load...
  bool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = true;
  rv = mDocShell->LoadURI(mURIToLoad, loadInfo,
                          nsIWebNavigation::LOAD_FLAGS_NONE, false);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nsnull;
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFrameLoader::CheckURILoad(nsIURI* aURI)
{
  // Check for security.  The fun part is trying to figure out what principals
  // to use.  The way I figure it, if we're doing a LoadFrame() accidentally
  // (eg someone created a frame/iframe node, we're being parsed, XUL iframes
  // are being reframed, etc.) then we definitely want to use the node
  // principal of mOwnerContent for security checks.  If, on the other hand,
  // someone's setting the src on our owner content, or created it via script,
  // or whatever, then they can clearly access it... and we should still use
  // the principal of mOwnerContent.  I don't think that leads to privilege
  // escalation, and it's reasonably guaranteed to not lead to XSS issues
  // (since caller can already access mOwnerContent in this case).  So just use
  // the principal of mOwnerContent no matter what.  If script wants to run
  // things with its own permissions, which differ from those of mOwnerContent
  // (which means the script is privileged in some way) it should set
  // window.location instead.
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

  // Get our principal
  nsIPrincipal* principal = mOwnerContent->NodePrincipal();

  // Check if we are allowed to load absURL
  nsresult rv =
    secMan->CheckLoadURIWithPrincipal(principal, aURI,
                                      nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  // Bail out if this is an infinite recursion scenario
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (mRemoteFrame) {
    return NS_OK;
  }
  return CheckForRecursiveLoad(aURI);
}

NS_IMETHODIMP
nsFrameLoader::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nsnull;
  nsresult rv = NS_OK;

  // If we have an owner, make sure we have a docshell and return
  // that. If not, we're most likely in the middle of being torn down,
  // then we just return null.
  if (mOwnerContent) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv))
      return rv;
    if (mRemoteFrame) {
      NS_WARNING("No docshells for remote frames!");
      return rv;
    }
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
  }

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);

  return rv;
}

void
nsFrameLoader::Finalize()
{
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (base_win) {
    base_win->Destroy();
  }
  mDocShell = nsnull;
}

static void
FirePageHideEvent(nsIDocShellTreeItem* aItem,
                  nsIDOMEventTarget* aChromeEventHandler)
{
  nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(aItem);
  nsCOMPtr<nsIDocument> internalDoc = do_QueryInterface(doc);
  NS_ASSERTION(internalDoc, "What happened here?");
  internalDoc->OnPageHide(true, aChromeEventHandler);

  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (PRUint32 i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageHideEvent(kids[i], aChromeEventHandler);
    }
  }
}

// The pageshow event is fired for a given document only if IsShowing() returns
// the same thing as aFireIfShowing.  This gives us a way to fire pageshow only
// on documents that are still loading or only on documents that are already
// loaded.
static void
FirePageShowEvent(nsIDocShellTreeItem* aItem,
                  nsIDOMEventTarget* aChromeEventHandler,
                  bool aFireIfShowing)
{
  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (PRUint32 i = 0; i < kids.Length(); ++i) {
    if (kids[i]) {
      FirePageShowEvent(kids[i], aChromeEventHandler, aFireIfShowing);
    }
  }

  nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(aItem);
  nsCOMPtr<nsIDocument> internalDoc = do_QueryInterface(doc);
  NS_ASSERTION(internalDoc, "What happened here?");
  if (internalDoc->IsShowing() == aFireIfShowing) {
    internalDoc->OnPageShow(true, aChromeEventHandler);
  }
}

static void
SetTreeOwnerAndChromeEventHandlerOnDocshellTree(nsIDocShellTreeItem* aItem,
                                                nsIDocShellTreeOwner* aOwner,
                                                nsIDOMEventTarget* aHandler)
{
  NS_PRECONDITION(aItem, "Must have item");

  aItem->SetTreeOwner(aOwner);
  nsCOMPtr<nsIDocShell> shell(do_QueryInterface(aItem));
  shell->SetChromeEventHandler(aHandler);

  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetChildAt(i, getter_AddRefs(item));
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

/**
 * Set the type of the treeitem and hook it up to the treeowner.
 * @param aItem the treeitem we're working with
 * @param aOwningContent the content node that owns aItem
 * @param aTreeOwner the relevant treeowner; might be null
 * @param aParentType the nsIDocShellTreeItem::GetType of our parent docshell
 * @param aParentNode if non-null, the docshell we should be added as a child to
 *
 * @return whether aItem is top-level content
 */
static bool
AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem, nsIContent* aOwningContent,
                       nsIDocShellTreeOwner* aOwner, PRInt32 aParentType,
                       nsIDocShellTreeNode* aParentNode)
{
  NS_PRECONDITION(aItem, "Must have docshell treeitem");
  NS_PRECONDITION(aOwningContent, "Must have owning content");
  
  nsAutoString value;
  bool isContent = false;

  if (aOwningContent->IsXUL()) {
      aOwningContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);
  } else {
      aOwningContent->GetAttr(kNameSpaceID_None, nsGkAtoms::mozframetype, value);
  }

  // we accept "content" and "content-xxx" values.
  // at time of writing, we expect "xxx" to be "primary" or "targetable", but
  // someday it might be an integer expressing priority or something else.

  isContent = value.LowerCaseEqualsLiteral("content") ||
    StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                     nsCaseInsensitiveStringComparator());

  // Force mozbrowser frames to always be typeContent, even if the
  // mozbrowser interfaces are disabled.
  nsCOMPtr<nsIDOMMozBrowserFrame> mozbrowser =
    do_QueryInterface(aOwningContent);
  if (mozbrowser) {
    bool isMozbrowser = false;
    mozbrowser->GetMozbrowser(&isMozbrowser);
    isContent |= isMozbrowser;
  }

  if (isContent) {
    // The web shell's type is content.

    aItem->SetItemType(nsIDocShellTreeItem::typeContent);
  } else {
    // Inherit our type from our parent docshell.  If it is
    // chrome, we'll be chrome.  If it is content, we'll be
    // content.

    aItem->SetItemType(aParentType);
  }

  // Now that we have our type set, add ourselves to the parent, as needed.
  if (aParentNode) {
    aParentNode->AddChild(aItem);
  }

  bool retval = false;
  if (aParentType == nsIDocShellTreeItem::typeChrome && isContent) {
    retval = true;

    bool is_primary = value.LowerCaseEqualsLiteral("content-primary");

    if (aOwner) {
      bool is_targetable = is_primary ||
        value.LowerCaseEqualsLiteral("content-targetable");
      aOwner->ContentShellAdded(aItem, is_primary, is_targetable, value);
    }
  }

  return retval;
}

static bool
AllDescendantsOfType(nsIDocShellTreeItem* aParentItem, PRInt32 aType)
{
  PRInt32 childCount = 0;
  aParentItem->GetChildCount(&childCount);

  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> kid;
    aParentItem->GetChildAt(i, getter_AddRefs(kid));

    PRInt32 kidType;
    kid->GetItemType(&kidType);
    if (kidType != aType || !AllDescendantsOfType(kid, aType)) {
      return false;
    }
  }

  return true;
}

/**
 * A class that automatically sets mInShow to false when it goes
 * out of scope.
 */
class NS_STACK_CLASS AutoResetInShow {
  private:
    nsFrameLoader* mFrameLoader;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    AutoResetInShow(nsFrameLoader* aFrameLoader MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mFrameLoader(aFrameLoader)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoResetInShow() { mFrameLoader->mInShow = false; }
};


bool
nsFrameLoader::Show(PRInt32 marginWidth, PRInt32 marginHeight,
                    PRInt32 scrollbarPrefX, PRInt32 scrollbarPrefY,
                    nsSubDocumentFrame* frame)
{
  if (mInShow) {
    return false;
  }
  // Reset mInShow if we exit early.
  AutoResetInShow resetInShow(this);
  mInShow = true;

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!mRemoteFrame) {
    if (!mDocShell)
      return false;
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell)
      return true;

    mDocShell->SetMarginWidth(marginWidth);
    mDocShell->SetMarginHeight(marginHeight);

    nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mDocShell);
    if (sc) {
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                         scrollbarPrefX);
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                         scrollbarPrefY);
    }
  }

  nsIView* view = frame->EnsureInnerView();
  if (!view)
    return false;

  if (mRemoteFrame) {
    return ShowRemoteFrame(GetSubDocumentSize(frame));
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWindow, "Found a nsIDocShell that isn't a nsIBaseWindow.");
  nsIntSize size;
  if (!(frame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // We have a useful size already; use it, since we might get no
    // more size updates.
    size = GetSubDocumentSize(frame);
  } else {
    // Pick some default size for now.  Using 10x10 because that's what the
    // code here used to do.
    size.SizeTo(10, 10);
  }
  baseWindow->InitWindow(nsnull, view->GetWidget(), 0, 0,
                         size.width, size.height);
  // This is kinda whacky, this "Create()" call doesn't really
  // create anything, one starts to wonder why this was named
  // "Create"...
  baseWindow->Create();
  baseWindow->SetVisibility(true);

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIDOMHTMLDocument> doc =
      do_QueryInterface(presShell->GetDocument());

    if (doc) {
      nsAutoString designMode;
      doc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        // Hold on to the editor object to let the document reattach to the
        // same editor object, instead of creating a new one.
        nsCOMPtr<nsIEditorDocShell> editorDocshell = do_QueryInterface(mDocShell);
        nsCOMPtr<nsIEditor> editor;
        nsresult rv = editorDocshell->GetEditor(getter_AddRefs(editor));
        NS_ENSURE_SUCCESS(rv, false);

        doc->SetDesignMode(NS_LITERAL_STRING("off"));
        doc->SetDesignMode(NS_LITERAL_STRING("on"));
      } else {
        // Re-initialize the presentation for contenteditable documents
        nsCOMPtr<nsIEditorDocShell> editorDocshell = do_QueryInterface(mDocShell);
        if (editorDocshell) {
          bool editable = false,
                 hasEditingSession = false;
          editorDocshell->GetEditable(&editable);
          editorDocshell->GetHasEditingSession(&hasEditingSession);
          nsCOMPtr<nsIEditor> editor;
          editorDocshell->GetEditor(getter_AddRefs(editor));
          if (editable && hasEditingSession && editor) {
            editor->PostCreate();
          }
        }
      }
    }
  }

  mInShow = false;
  if (mHideCalled) {
    mHideCalled = false;
    Hide();
    return false;
  }
  return true;
}

void
nsFrameLoader::MarginsChanged(PRUint32 aMarginWidth,
                              PRUint32 aMarginHeight)
{
  // We assume that the margins are always zero for remote frames.
  if (mRemoteFrame)
    return;

  // If there's no docshell, we're probably not up and running yet.
  // nsFrameLoader::Show() will take care of setting the right
  // margins.
  if (!mDocShell)
    return;

  // Set the margins
  mDocShell->SetMarginWidth(aMarginWidth);
  mDocShell->SetMarginHeight(aMarginHeight);

  // Trigger a restyle if there's a prescontext
  nsRefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext)
    presContext->RebuildAllStyleData(nsChangeHint(0));
}

bool
nsFrameLoader::ShowRemoteFrame(const nsIntSize& size)
{
  NS_ASSERTION(mRemoteFrame, "ShowRemote only makes sense on remote frames.");

  if (!mRemoteBrowser) {
    TryRemoteBrowser();

    if (!mRemoteBrowser) {
      NS_ERROR("Couldn't create child process.");
      return false;
    }
  }

  // FIXME/bug 589337: Show()/Hide() is pretty expensive for
  // cross-process layers; need to figure out what behavior we really
  // want here.  For now, hack.
  if (!mRemoteBrowserShown) {
    if (!mOwnerContent ||
        !mOwnerContent->GetCurrentDoc()) {
      return false;
    }

    nsRefPtr<layers::LayerManager> layerManager =
      nsContentUtils::LayerManagerForDocument(mOwnerContent->GetCurrentDoc());
    if (!layerManager) {
      // This is just not going to work.
      return false;
    }

    mRemoteBrowser->Show(size);
    mRemoteBrowserShown = true;

    EnsureMessageManager();

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (OwnerIsBrowserFrame() && os) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                          "remote-browser-frame-shown", NULL);
    }
  } else {
    nsRect dimensions;
    NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), false);
    mRemoteBrowser->UpdateDimensions(dimensions, size);
  }

  return true;
}

void
nsFrameLoader::Hide()
{
  if (mHideCalled) {
    return;
  }
  if (mInShow) {
    mHideCalled = true;
    return;
  }

  if (!mDocShell)
    return;

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer)
    contentViewer->SetSticky(false);

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWin,
               "Found an nsIDocShell which doesn't implement nsIBaseWindow.");
  baseWin->SetVisibility(false);
  baseWin->SetParentWidget(nsnull);
}

nsresult
nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                   nsRefPtr<nsFrameLoader>& aFirstToSwap,
                                   nsRefPtr<nsFrameLoader>& aSecondToSwap)
{
  NS_PRECONDITION((aFirstToSwap == this && aSecondToSwap == aOther) ||
                  (aFirstToSwap == aOther && aSecondToSwap == this),
                  "Swapping some sort of random loaders?");
  NS_ENSURE_STATE(!mInShow && !aOther->mInShow);

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv =
    ourContent->NodePrincipal()->Equals(otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIDocShell> ourDocshell = GetExistingDocShell();
  nsCOMPtr<nsIDocShell> otherDocshell = aOther->GetExistingDocShell();
  if (!ourDocshell || !otherDocshell) {
    // How odd
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // To avoid having to mess with session history, avoid swapping
  // frameloaders that don't correspond to root same-type docshells,
  // unless both roots have session history disabled.
  nsCOMPtr<nsIDocShellTreeItem> ourTreeItem = do_QueryInterface(ourDocshell);
  nsCOMPtr<nsIDocShellTreeItem> otherTreeItem =
    do_QueryInterface(otherDocshell);
  nsCOMPtr<nsIDocShellTreeItem> ourRootTreeItem, otherRootTreeItem;
  ourTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(ourRootTreeItem));
  otherTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(otherRootTreeItem));
  nsCOMPtr<nsIWebNavigation> ourRootWebnav =
    do_QueryInterface(ourRootTreeItem);
  nsCOMPtr<nsIWebNavigation> otherRootWebnav =
    do_QueryInterface(otherRootTreeItem);

  if (!ourRootWebnav || !otherRootWebnav) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsISHistory> ourHistory;
  nsCOMPtr<nsISHistory> otherHistory;
  ourRootWebnav->GetSessionHistory(getter_AddRefs(ourHistory));
  otherRootWebnav->GetSessionHistory(getter_AddRefs(otherHistory));

  if ((ourRootTreeItem != ourTreeItem || otherRootTreeItem != otherTreeItem) &&
      (ourHistory || otherHistory)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Also make sure that the two docshells are the same type. Otherwise
  // swapping is certainly not safe.
  PRInt32 ourType = nsIDocShellTreeItem::typeChrome;
  PRInt32 otherType = nsIDocShellTreeItem::typeChrome;
  ourTreeItem->GetItemType(&ourType);
  otherTreeItem->GetItemType(&otherType);
  if (ourType != otherType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // One more twist here.  Setting up the right treeowners in a heterogeneous
  // tree is a bit of a pain.  So make sure that if ourType is not
  // nsIDocShellTreeItem::typeContent then all of our descendants are the same
  // type as us.
  if (ourType != nsIDocShellTreeItem::typeContent &&
      (!AllDescendantsOfType(ourTreeItem, ourType) ||
       !AllDescendantsOfType(otherTreeItem, otherType))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  
  // Save off the tree owners, frame elements, chrome event handlers, and
  // docshell and document parents before doing anything else.
  nsCOMPtr<nsIDocShellTreeOwner> ourOwner, otherOwner;
  ourTreeItem->GetTreeOwner(getter_AddRefs(ourOwner));
  otherTreeItem->GetTreeOwner(getter_AddRefs(otherOwner));
  // Note: it's OK to have null treeowners.

  nsCOMPtr<nsIDocShellTreeItem> ourParentItem, otherParentItem;
  ourTreeItem->GetParent(getter_AddRefs(ourParentItem));
  otherTreeItem->GetParent(getter_AddRefs(otherParentItem));
  if (!ourParentItem || !otherParentItem) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure our parents are the same type too
  PRInt32 ourParentType = nsIDocShellTreeItem::typeContent;
  PRInt32 otherParentType = nsIDocShellTreeItem::typeContent;
  ourParentItem->GetItemType(&ourParentType);
  otherParentItem->GetItemType(&otherParentType);
  if (ourParentType != otherParentType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsPIDOMWindow> ourWindow = do_GetInterface(ourDocshell);
  nsCOMPtr<nsPIDOMWindow> otherWindow = do_GetInterface(otherDocshell);

  nsCOMPtr<nsIDOMElement> ourFrameElement =
    ourWindow->GetFrameElementInternal();
  nsCOMPtr<nsIDOMElement> otherFrameElement =
    otherWindow->GetFrameElementInternal();

  nsCOMPtr<nsIDOMEventTarget> ourChromeEventHandler =
    do_QueryInterface(ourWindow->GetChromeEventHandler());
  nsCOMPtr<nsIDOMEventTarget> otherChromeEventHandler =
    do_QueryInterface(otherWindow->GetChromeEventHandler());

  NS_ASSERTION(SameCOMIdentity(ourFrameElement, ourContent) &&
               SameCOMIdentity(otherFrameElement, otherContent) &&
               SameCOMIdentity(ourChromeEventHandler, ourContent) &&
               SameCOMIdentity(otherChromeEventHandler, otherContent),
               "How did that happen, exactly?");

  nsCOMPtr<nsIDocument> ourChildDocument =
    do_QueryInterface(ourWindow->GetExtantDocument());
  nsCOMPtr<nsIDocument> otherChildDocument =
    do_QueryInterface(otherWindow->GetExtantDocument());
  if (!ourChildDocument || !otherChildDocument) {
    // This shouldn't be happening
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDocument> ourParentDocument =
    ourChildDocument->GetParentDocument();
  nsCOMPtr<nsIDocument> otherParentDocument =
    otherChildDocument->GetParentDocument();

  // Make sure to swap docshells between the two frames.
  nsIDocument* ourDoc = ourContent->GetCurrentDoc();
  nsIDocument* otherDoc = otherContent->GetCurrentDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_ASSERTION(ourDoc == ourParentDocument, "Unexpected parent document");
  NS_ASSERTION(otherDoc == otherParentDocument, "Unexpected parent document");

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool weAreBrowserFrame = false;
  bool otherIsBrowserFrame = false;
  ourDocshell->GetIsBrowserFrame(&weAreBrowserFrame);
  otherDocshell->GetIsBrowserFrame(&otherIsBrowserFrame);
  if (weAreBrowserFrame != otherIsBrowserFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mInSwap = aOther->mInSwap = true;

  // Fire pageshow events on still-loading pages, and then fire pagehide
  // events.  Note that we do NOT fire these in the normal way, but just fire
  // them on the chrome event handlers.
  FirePageShowEvent(ourTreeItem, ourChromeEventHandler, false);
  FirePageShowEvent(otherTreeItem, otherChromeEventHandler, false);
  FirePageHideEvent(ourTreeItem, ourChromeEventHandler);
  FirePageHideEvent(otherTreeItem, otherChromeEventHandler);
  
  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = false;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, true);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, true);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    mInSwap = aOther->mInSwap = false;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, true);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, true);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // OK.  First begin to swap the docshells in the two nsIFrames
  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    mInSwap = aOther->mInSwap = false;
    FirePageShowEvent(ourTreeItem, ourChromeEventHandler, true);
    FirePageShowEvent(otherTreeItem, otherChromeEventHandler, true);
    return rv;
  }

  // Now move the docshells to the right docshell trees.  Note that this
  // resets their treeowners to null.
  ourParentItem->RemoveChild(ourTreeItem);
  otherParentItem->RemoveChild(otherTreeItem);
  if (ourType == nsIDocShellTreeItem::typeContent) {
    ourOwner->ContentShellRemoved(ourTreeItem);
    otherOwner->ContentShellRemoved(otherTreeItem);
  }
  
  ourParentItem->AddChild(otherTreeItem);
  otherParentItem->AddChild(ourTreeItem);

  // Restore the correct treeowners
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(ourTreeItem, otherOwner,
                                                  otherChromeEventHandler);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(otherTreeItem, ourOwner,
                                                  ourChromeEventHandler);

  AddTreeItemToTreeOwner(ourTreeItem, otherContent, otherOwner,
                         otherParentType, nsnull);
  AddTreeItemToTreeOwner(otherTreeItem, ourContent, ourOwner, ourParentType,
                         nsnull);

  // SetSubDocumentFor nulls out parent documents on the old child doc if a
  // new non-null document is passed in, so just go ahead and remove both
  // kids before reinserting in the parent subdoc maps, to avoid
  // complications.
  ourParentDocument->SetSubDocumentFor(ourContent, nsnull);
  otherParentDocument->SetSubDocumentFor(otherContent, nsnull);
  ourParentDocument->SetSubDocumentFor(ourContent, otherChildDocument);
  otherParentDocument->SetSubDocumentFor(otherContent, ourChildDocument);

  ourWindow->SetFrameElementInternal(otherFrameElement);
  otherWindow->SetFrameElementInternal(ourFrameElement);

  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  nsRefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  nsRefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap pointers in child message managers.
  if (mChildMessageManager) {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
    tabChild->SetOwner(otherContent);
    tabChild->SetChromeMessageManager(otherMessageManager);
  }
  if (aOther->mChildMessageManager) {
    nsInProcessTabChildGlobal* otherTabChild =
      static_cast<nsInProcessTabChildGlobal*>(aOther->mChildMessageManager.get());
    otherTabChild->SetOwner(ourContent);
    otherTabChild->SetChromeMessageManager(ourMessageManager);
  }
  // Swap and setup things in parent message managers.
  nsFrameMessageManager* ourParentManager = mMessageManager ?
    mMessageManager->GetParentManager() : nsnull;
  nsFrameMessageManager* otherParentManager = aOther->mMessageManager ?
    aOther->mMessageManager->GetParentManager() : nsnull;
  JSContext* thisCx =
    mMessageManager ? mMessageManager->GetJSContext() : nsnull;
  JSContext* otherCx = 
    aOther->mMessageManager ? aOther->mMessageManager->GetJSContext() : nsnull;
  if (mMessageManager) {
    mMessageManager->RemoveFromParent();
    mMessageManager->SetJSContext(otherCx);
    mMessageManager->SetParentManager(otherParentManager);
    mMessageManager->SetCallbackData(aOther, false);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->RemoveFromParent();
    aOther->mMessageManager->SetJSContext(thisCx);
    aOther->mMessageManager->SetParentManager(ourParentManager);
    aOther->mMessageManager->SetCallbackData(this, false);
  }
  mMessageManager.swap(aOther->mMessageManager);

  aFirstToSwap.swap(aSecondToSwap);

  // Drop any cached content viewers in the two session histories.
  nsCOMPtr<nsISHistoryInternal> ourInternalHistory =
    do_QueryInterface(ourHistory);
  nsCOMPtr<nsISHistoryInternal> otherInternalHistory =
    do_QueryInterface(otherHistory);
  if (ourInternalHistory) {
    ourInternalHistory->EvictAllContentViewers();
  }
  if (otherInternalHistory) {
    otherInternalHistory->EvictAllContentViewers();
  }

  NS_ASSERTION(ourFrame == ourContent->GetPrimaryFrame() &&
               otherFrame == otherContent->GetPrimaryFrame(),
               "changed primary frame");

  ourFrameFrame->EndSwapDocShells(otherFrame);

  ourParentDocument->FlushPendingNotifications(Flush_Layout);
  otherParentDocument->FlushPendingNotifications(Flush_Layout);

  FirePageShowEvent(ourTreeItem, otherChromeEventHandler, true);
  FirePageShowEvent(otherTreeItem, ourChromeEventHandler, true);

  mInSwap = aOther->mInSwap = false;
  return NS_OK;
}

void
nsFrameLoader::DestroyChild()
{
  if (mRemoteBrowser) {
    mRemoteBrowser->SetOwnerElement(nsnull);
    mRemoteBrowser->Destroy();
    mRemoteBrowser = nsnull;
  }
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  if (mDestroyCalled) {
    return NS_OK;
  }
  mDestroyCalled = true;

  if (mMessageManager) {
    mMessageManager->Disconnect();
  }
  if (mChildMessageManager) {
    static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get())->Disconnect();
  }

  nsCOMPtr<nsIDocument> doc;
  bool dynamicSubframeRemoval = false;
  if (mOwnerContent) {
    doc = mOwnerContent->OwnerDoc();
    dynamicSubframeRemoval = !mIsTopLevelContent && !doc->InUnlinkOrDeletion();
    doc->SetSubDocumentFor(mOwnerContent, nsnull);

    SetOwnerContent(nsnull);
  }
  DestroyChild();

  // Seems like this is a dynamic frame removal.
  if (dynamicSubframeRemoval) {
    nsCOMPtr<nsIDocShellHistory> dhistory = do_QueryInterface(mDocShell);
    if (dhistory) {
      dhistory->RemoveFromSessionHistory();
    }
  }

  // Let the tree owner know we're gone.
  if (mIsTopLevelContent) {
    nsCOMPtr<nsIDocShellTreeItem> ourItem = do_QueryInterface(mDocShell);
    if (ourItem) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      ourItem->GetParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsIDocShellTreeOwner> owner = do_GetInterface(parentItem);
      if (owner) {
        owner->ContentShellRemoved(ourItem);
      }
    }
  }
  
  // Let our window know that we are gone
  nsCOMPtr<nsPIDOMWindow> win_private(do_GetInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(nsnull);
  }

  if ((mNeedsAsyncDestroy || !doc ||
       NS_FAILED(doc->FinalizeFrameLoader(this))) && mDocShell) {
    nsCOMPtr<nsIRunnable> event = new nsAsyncDocShellDestroyer(mDocShell);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    NS_DispatchToCurrentThread(event);

    // Let go of our docshell now that the async destroyer holds on to
    // the docshell.

    mDocShell = nsnull;
  }

  // NOTE: 'this' may very well be gone by now.

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetDepthTooGreat(bool* aDepthTooGreat)
{
  *aDepthTooGreat = mDepthTooGreat;
  return NS_OK;
}

void
nsFrameLoader::SetOwnerContent(Element* aContent)
{
  mOwnerContent = aContent;
  if (RenderFrameParent* rfp = GetCurrentRemoteFrame()) {
    rfp->OwnerContentChanged(aContent);
  }
}

bool
nsFrameLoader::OwnerIsBrowserFrame()
{
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  bool isBrowser = false;
  if (browserFrame) {
    browserFrame->GetReallyIsBrowser(&isBrowser);
  }
  return isBrowser;
}

bool
nsFrameLoader::ShouldUseRemoteProcess()
{
  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS") ||
      Preferences::GetBool("dom.ipc.tabs.disabled", false)) {
    return false;
  }

  // If we're an <iframe mozbrowser> and we don't have a "remote" attribute,
  // fall back to the default.
  if (OwnerIsBrowserFrame() &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::Remote)) {

    return Preferences::GetBool("dom.ipc.browser_frames.oop_by_default", false);
  }

  // Otherwise, we're remote if we have "remote=true" and we're either a
  // browser frame or a XUL element.
  return (OwnerIsBrowserFrame() ||
          mOwnerContent->GetNameSpaceID() == kNameSpaceID_XUL) &&
         mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::Remote,
                                    nsGkAtoms::_true,
                                    eCaseMatters);
}

nsresult
nsFrameLoader::MaybeCreateDocShell()
{
  if (mDocShell) {
    return NS_OK;
  }
  if (mRemoteFrame) {
    return NS_OK;
  }
  NS_ENSURE_STATE(!mDestroyCalled);

  if (ShouldUseRemoteProcess()) {
    mRemoteFrame = true;
    return NS_OK;
  }

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (!(doc->IsStaticDocument() || mOwnerContent->IsInDoc())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (doc->IsResourceDoc() || !doc->IsActive()) {
    // Don't allow subframe loads in resource documents, nor
    // in non-active documents.
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> container =
    doc->GetContainer();
  nsCOMPtr<nsIWebNavigation> parentAsWebNav = do_QueryInterface(container);
  NS_ENSURE_STATE(parentAsWebNav);

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  if (!mNetworkCreated) {
    nsCOMPtr<nsIDocShellHistory> history = do_QueryInterface(mDocShell);
    if (history) {
      history->SetCreatedDynamically(true);
    }
  }

  // Get the frame name and tell the docshell about it.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsAutoString frameName;

  PRInt32 namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML && !mOwnerContent->IsInHTMLDocument()) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    // XXX if no NAME then use ID, after a transition period this will be
    // changed so that XUL only uses ID too (bug 254284).
    if (frameName.IsEmpty() && namespaceID == kNameSpaceID_XUL) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
    }
  }

  if (!frameName.IsEmpty()) {
    docShellAsItem->SetName(frameName.get());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.

  nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(parentAsWebNav));
  if (parentAsNode) {
    // Note: This logic duplicates a lot of logic in
    // nsSubDocumentFrame::AttributeChanged.  We should fix that.

    nsCOMPtr<nsIDocShellTreeItem> parentAsItem =
      do_QueryInterface(parentAsNode);

    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);

    // XXXbz why is this in content code, exactly?  We should handle
    // this some other way.....  Not sure how yet.
    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    NS_ENSURE_STATE(parentTreeOwner);
    mIsTopLevelContent =
      AddTreeItemToTreeOwner(docShellAsItem, mOwnerContent, parentTreeOwner,
                             parentType, parentAsNode);

    // Make sure all shells have links back to the content element
    // in the nearest enclosing chrome shell.
    nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;

    if (parentType == nsIDocShellTreeItem::typeChrome) {
      // Our parent shell is a chrome shell. It is therefore our nearest
      // enclosing chrome shell.

      chromeEventHandler = do_QueryInterface(mOwnerContent);
      NS_ASSERTION(chromeEventHandler,
                   "This mContent should implement this.");
    } else {
      nsCOMPtr<nsIDocShell> parentShell(do_QueryInterface(parentAsNode));

      // Our parent shell is a content shell. Get the chrome event
      // handler from it and use that for our shell as well.

      parentShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
    }

    mDocShell->SetChromeEventHandler(chromeEventHandler);
  }

  // This is nasty, this code (the do_GetInterface(mDocShell) below)
  // *must* come *after* the above call to
  // mDocShell->SetChromeEventHandler() for the global window to get
  // the right chrome event handler.

  // Tell the window about the frame that hosts it.
  nsCOMPtr<nsIDOMElement> frame_element(do_QueryInterface(mOwnerContent));
  NS_ASSERTION(frame_element, "frame loader owner element not a DOM element!");

  nsCOMPtr<nsPIDOMWindow> win_private(do_GetInterface(mDocShell));
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(frame_element);
  }

  // This is kinda whacky, this call doesn't really create anything,
  // but it must be called to make sure things are properly
  // initialized.
  if (NS_FAILED(base_win->Create()) || !win_private) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  EnsureMessageManager();

  if (OwnerIsBrowserFrame()) {
    mDocShell->SetIsBrowserFrame(true);

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                          "in-process-browser-frame-shown", NULL);
    }

    if (mMessageManager) {
      mMessageManager->LoadFrameScript(
        NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js"),
        /* allowDelayedLoad = */ true);
    }
  }

  return NS_OK;
}

void
nsFrameLoader::GetURL(nsString& aURI)
{
  aURI.Truncate();

  if (mOwnerContent->Tag() == nsGkAtoms::object) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aURI);
  }
}

nsresult
nsFrameLoader::CheckForRecursiveLoad(nsIURI* aURI)
{
  nsresult rv;

  mDepthTooGreat = false;
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(!mRemoteFrame,
               "Shouldn't call CheckForRecursiveLoad on remote frames.");
  if (!mDocShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  NS_ASSERTION(treeItem, "docshell must be a treeitem!");

  // Check that we're still in the docshell tree.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_WARN_IF_FALSE(treeOwner,
                   "Trying to load a new url to a docshell without owner!");
  NS_ENSURE_STATE(treeOwner);
  
  
  PRInt32 ourType;
  rv = treeItem->GetItemType(&ourType);
  if (NS_SUCCEEDED(rv) && ourType != nsIDocShellTreeItem::typeContent) {
    // No need to do recursion-protection here XXXbz why not??  Do we really
    // trust people not to screw up with non-content docshells?
    return NS_OK;
  }

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  treeItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
  PRInt32 depth = 0;
  while (parentAsItem) {
    ++depth;
    
    if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
      mDepthTooGreat = true;
      NS_WARNING("Too many nested content frames so giving up");

      return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)
    }

    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }
  
  // Bug 136580: Check for recursive frame loading
  PRInt32 matchCount = 0;
  treeItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
  while (parentAsItem) {
    // Check the parent URI with the URI we're loading
    nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentAsItem));
    if (parentAsNav) {
      // Does the URI match the one we're about to load?
      nsCOMPtr<nsIURI> parentURI;
      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (parentURI) {
        // Bug 98158/193011: We need to ignore data after the #
        bool equal;
        rv = aURI->EqualsExceptRef(parentURI, &equal);
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (equal) {
          matchCount++;
          if (matchCount >= MAX_SAME_URL_CONTENT_FRAMES) {
            NS_WARNING("Too many nested content frames have the same url (recursion?) so giving up");
            return NS_ERROR_UNEXPECTED;
          }
        }
      }
    }
    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  return NS_OK;
}

nsresult
nsFrameLoader::GetWindowDimensions(nsRect& aRect)
{
  // Need to get outer window position here
  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  if (doc->GetDisplayDocument()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebNavigation> parentAsWebNav =
    do_GetInterface(doc->GetScriptGlobalObject());

  if (!parentAsWebNav) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsWebNav));

  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentAsItem->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_GetInterface(parentOwner));
  treeOwnerAsWin->GetPosition(&aRect.x, &aRect.y);
  treeOwnerAsWin->GetSize(&aRect.width, &aRect.height);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::UpdatePositionAndSize(nsIFrame *aIFrame)
{
  if (mRemoteFrame) {
    if (mRemoteBrowser) {
      nsIntSize size = GetSubDocumentSize(aIFrame);
      nsRect dimensions;
      NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), NS_ERROR_FAILURE);
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
    return NS_OK;
  }
  return UpdateBaseWindowPositionAndSize(aIFrame);
}

nsresult
nsFrameLoader::UpdateBaseWindowPositionAndSize(nsIFrame *aIFrame)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  // resize the sub document
  if (baseWindow) {
    PRInt32 x = 0;
    PRInt32 y = 0;

    nsWeakFrame weakFrame(aIFrame);

    baseWindow->GetPositionAndSize(&x, &y, nsnull, nsnull);

    if (!weakFrame.IsAlive()) {
      // GetPositionAndSize() killed us
      return NS_OK;
    }

    nsIntSize size = GetSubDocumentSize(aIFrame);

    baseWindow->SetPositionAndSize(x, y, size.width, size.height, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetRenderMode(PRUint32* aRenderMode)
{
  *aRenderMode = mRenderMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetRenderMode(PRUint32 aRenderMode)
{
  if (aRenderMode == mRenderMode) {
    return NS_OK;
  }

  mRenderMode = aRenderMode;
  // NB: we pass INVALIDATE_NO_THEBES_LAYERS here to keep view
  // semantics the same for both in-process and out-of-process
  // <browser>.  This is just a transform of the layer subtree in
  // both.
  InvalidateFrame(GetPrimaryFrameOfOwningContent(), nsIFrame::INVALIDATE_NO_THEBES_LAYERS);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetEventMode(PRUint32* aEventMode)
{
  *aEventMode = mEventMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetEventMode(PRUint32 aEventMode)
{
  mEventMode = aEventMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetClipSubdocument(bool* aResult)
{
  *aResult = mClipSubdocument;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetClipSubdocument(bool aClip)
{
  mClipSubdocument = aClip;
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    InvalidateFrame(frame, 0);
    frame->PresContext()->PresShell()->
      FrameNeedsReflow(frame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(frame);
    if (subdocFrame) {
      nsIFrame* subdocRootFrame = subdocFrame->GetSubdocumentRootFrame();
      if (subdocRootFrame) {
        nsIFrame* subdocRootScrollFrame = subdocRootFrame->PresContext()->PresShell()->
          GetRootScrollFrame();
        if (subdocRootScrollFrame) {
          frame->PresContext()->PresShell()->
            FrameNeedsReflow(frame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetClampScrollPosition(bool* aResult)
{
  *aResult = mClampScrollPosition;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetClampScrollPosition(bool aClamp)
{
  mClampScrollPosition = aClamp;

  // When turning clamping on, make sure the current position is clamped.
  if (aClamp) {
    nsIFrame* frame = GetPrimaryFrameOfOwningContent();
    if (frame) {
      nsSubDocumentFrame* subdocFrame = do_QueryFrame(frame);
      if (subdocFrame) {
        nsIFrame* subdocRootFrame = subdocFrame->GetSubdocumentRootFrame();
        if (subdocRootFrame) {
          nsIScrollableFrame* subdocRootScrollFrame = subdocRootFrame->PresContext()->PresShell()->
            GetRootScrollFrameAsScrollable();
          if (subdocRootScrollFrame) {
            subdocRootScrollFrame->ScrollTo(subdocRootScrollFrame->GetScrollPosition(), nsIScrollableFrame::INSTANT);
          }
        }
      }
    }
  }
  return NS_OK;
}

nsIntSize
nsFrameLoader::GetSubDocumentSize(const nsIFrame *aIFrame)
{
  nsSize docSizeAppUnits;
  nsPresContext* presContext = aIFrame->PresContext();
  nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = 
    do_QueryInterface(aIFrame->GetContent());
  if (frameElem) {
    docSizeAppUnits = aIFrame->GetSize();
  } else {
    docSizeAppUnits = aIFrame->GetContentRect().Size();
  }
  return nsIntSize(presContext->AppUnitsToDevPixels(docSizeAppUnits.width),
                   presContext->AppUnitsToDevPixels(docSizeAppUnits.height));
}

bool
nsFrameLoader::TryRemoteBrowser()
{
  NS_ASSERTION(!mRemoteBrowser, "TryRemoteBrowser called with a remote browser already?");

  nsIDocument* doc = mOwnerContent->GetDocument();
  if (!doc) {
    return false;
  }

  if (doc->GetDisplayDocument()) {
    // Don't allow subframe loads in external reference documents
    return false;
  }

  nsCOMPtr<nsIWebNavigation> parentAsWebNav =
    do_GetInterface(doc->GetScriptGlobalObject());

  if (!parentAsWebNav) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsWebNav));

  // <iframe mozbrowser> gets to skip these checks.
  if (!OwnerIsBrowserFrame()) {
    PRInt32 parentType;
    parentAsItem->GetItemType(&parentType);

    if (parentType != nsIDocShellTreeItem::typeChrome) {
      return false;
    }

    if (!mOwnerContent->IsXUL()) {
      return false;
    }

    nsAutoString value;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);

    if (!value.LowerCaseEqualsLiteral("content") &&
        !StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                          nsCaseInsensitiveStringComparator())) {
      return false;
    }
  }

  PRUint32 chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentAsItem->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return false;
  }
  nsCOMPtr<nsIXULWindow> window(do_GetInterface(parentOwner));
  if (!window) {
    return false;
  }
  if (NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  ContentParent* parent = ContentParent::GetNewOrUsed();
  NS_ASSERTION(parent->IsAlive(), "Process parent should be alive; something is very wrong!");
  mRemoteBrowser = parent->CreateTab(chromeFlags,
                                     /* aIsBrowserFrame = */ OwnerIsBrowserFrame());
  if (mRemoteBrowser) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mOwnerContent);
    mRemoteBrowser->SetOwnerElement(element);

    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    parentAsItem->GetRootTreeItem(getter_AddRefs(rootItem));
    nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);
    NS_ABORT_IF_FALSE(rootChromeWin, "How did we not get a chrome window here?");

    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    mRemoteBrowser->SetBrowserDOMWindow(browserDOMWin);
    
    mChildHost = parent;
  }
  return true;
}

mozilla::dom::PBrowserParent*
nsFrameLoader::GetRemoteBrowser()
{
  return mRemoteBrowser;
}

NS_IMETHODIMP
nsFrameLoader::ActivateRemoteFrame() {
  if (mRemoteBrowser) {
    mRemoteBrowser->Activate();
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::DeactivateRemoteFrame() {
  if (mRemoteBrowser) {
    mRemoteBrowser->Deactivate();
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessMouseEvent(const nsAString& aType,
                                          float aX,
                                          float aY,
                                          PRInt32 aButton,
                                          PRInt32 aClickCount,
                                          PRInt32 aModifiers,
                                          bool aIgnoreRootScrollFrame)
{
  if (mRemoteBrowser) {
    mRemoteBrowser->SendMouseEvent(aType, aX, aY, aButton,
                                   aClickCount, aModifiers,
                                   aIgnoreRootScrollFrame);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::ActivateFrameEvent(const nsAString& aType,
                                  bool aCapture)
{
  if (mRemoteBrowser) {
    return mRemoteBrowser->SendActivateFrameEvent(nsString(aType), aCapture) ?
      NS_OK : NS_ERROR_NOT_AVAILABLE;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessKeyEvent(const nsAString& aType,
                                        PRInt32 aKeyCode,
                                        PRInt32 aCharCode,
                                        PRInt32 aModifiers,
                                        bool aPreventDefault)
{
  if (mRemoteBrowser) {
    mRemoteBrowser->SendKeyEvent(aType, aKeyCode, aCharCode, aModifiers,
                                 aPreventDefault);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::GetDelayRemoteDialogs(bool* aRetVal)
{
  *aRetVal = mDelayRemoteDialogs;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetDelayRemoteDialogs(bool aDelay)
{
  if (mRemoteBrowser && mDelayRemoteDialogs && !aDelay) {
    nsRefPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(mRemoteBrowser,
                           &mozilla::dom::TabParent::HandleDelayedDialogs);
    NS_DispatchToCurrentThread(ev);
  }
  mDelayRemoteDialogs = aDelay;
  return NS_OK;
}

nsresult
nsFrameLoader::CreateStaticClone(nsIFrameLoader* aDest)
{
  nsFrameLoader* dest = static_cast<nsFrameLoader*>(aDest);
  dest->MaybeCreateDocShell();
  NS_ENSURE_STATE(dest->mDocShell);

  nsCOMPtr<nsIDOMDocument> dummy = do_GetInterface(dest->mDocShell);
  nsCOMPtr<nsIContentViewer> viewer;
  dest->mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<nsIDocShell> origDocShell;
  GetDocShell(getter_AddRefs(origDocShell));
  nsCOMPtr<nsIDOMDocument> domDoc = do_GetInterface(origDocShell);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);
  nsCOMPtr<nsIDocument> clonedDoc = doc->CreateStaticClone(dest->mDocShell);
  nsCOMPtr<nsIDOMDocument> clonedDOMDoc = do_QueryInterface(clonedDoc);

  viewer->SetDOMDocument(clonedDOMDoc);
  return NS_OK;
}

bool LoadScript(void* aCallbackData, const nsAString& aURL)
{
  mozilla::dom::PBrowserParent* tabParent =
    static_cast<nsFrameLoader*>(aCallbackData)->GetRemoteBrowser();
  if (tabParent) {
    return tabParent->SendLoadRemoteScript(nsString(aURL));
  }
  nsFrameLoader* fl = static_cast<nsFrameLoader*>(aCallbackData);
  nsRefPtr<nsInProcessTabChildGlobal> tabChild =
    static_cast<nsInProcessTabChildGlobal*>(fl->GetTabChildGlobalAsEventTarget());
  if (tabChild) {
    tabChild->LoadFrameScript(aURL);
  }
  return true;
}

class nsAsyncMessageToChild : public nsRunnable
{
public:
  nsAsyncMessageToChild(nsFrameLoader* aFrameLoader,
                        const nsAString& aMessage, const nsAString& aJSON)
    : mFrameLoader(aFrameLoader), mMessage(aMessage), mJSON(aJSON) {}

  NS_IMETHOD Run()
  {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mFrameLoader->mChildMessageManager.get());
    if (tabChild && tabChild->GetInnerManager()) {
      nsFrameScriptCx cx(static_cast<nsIDOMEventTarget*>(tabChild), tabChild);
      nsRefPtr<nsFrameMessageManager> mm = tabChild->GetInnerManager();
      mm->ReceiveMessage(static_cast<nsIDOMEventTarget*>(tabChild), mMessage,
                         false, mJSON, nsnull, nsnull);
    }
    return NS_OK;
  }
  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsString mMessage;
  nsString mJSON;
};

bool SendAsyncMessageToChild(void* aCallbackData,
                             const nsAString& aMessage,
                             const nsAString& aJSON)
{
  mozilla::dom::PBrowserParent* tabParent =
    static_cast<nsFrameLoader*>(aCallbackData)->GetRemoteBrowser();
  if (tabParent) {
    return tabParent->SendAsyncMessage(nsString(aMessage), nsString(aJSON));
  }
  nsRefPtr<nsIRunnable> ev =
    new nsAsyncMessageToChild(static_cast<nsFrameLoader*>(aCallbackData),
                              aMessage, aJSON);
  NS_DispatchToCurrentThread(ev);
  return true;
}

NS_IMETHODIMP
nsFrameLoader::GetMessageManager(nsIChromeFrameMessageManager** aManager)
{
  EnsureMessageManager();
  if (mMessageManager) {
    CallQueryInterface(mMessageManager, aManager);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetContentViewsIn(float aXPx, float aYPx,
                                 float aTopSize, float aRightSize,
                                 float aBottomSize, float aLeftSize,
                                 PRUint32* aLength,
                                 nsIContentView*** aResult)
{
  nscoord x = nsPresContext::CSSPixelsToAppUnits(aXPx - aLeftSize);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aYPx - aTopSize);
  nscoord w = nsPresContext::CSSPixelsToAppUnits(aLeftSize + aRightSize) + 1;
  nscoord h = nsPresContext::CSSPixelsToAppUnits(aTopSize + aBottomSize) + 1;
  nsRect target(x, y, w, h);

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();

  nsTArray<ViewID> ids;
  nsLayoutUtils::GetRemoteContentIds(frame, target, ids, true);
  if (ids.Length() == 0 || !GetCurrentRemoteFrame()) {
    *aResult = nsnull;
    *aLength = 0;
    return NS_OK;
  }

  nsIContentView** result = reinterpret_cast<nsIContentView**>(
    NS_Alloc(ids.Length() * sizeof(nsIContentView*)));

  for (PRUint32 i = 0; i < ids.Length(); i++) {
    nsIContentView* view = GetCurrentRemoteFrame()->GetContentView(ids[i]);
    NS_ABORT_IF_FALSE(view, "Retrieved ID from RenderFrameParent, it should be valid!");
    nsRefPtr<nsIContentView>(view).forget(&result[i]);
  }

  *aResult = result;
  *aLength = ids.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetRootContentView(nsIContentView** aContentView)
{
  RenderFrameParent* rfp = GetCurrentRemoteFrame();
  if (!rfp) {
    *aContentView = nsnull;
    return NS_OK;
  }

  nsContentView* view = rfp->GetContentView();
  NS_ABORT_IF_FALSE(view, "Should always be able to create root scrollable!");
  nsRefPtr<nsIContentView>(view).forget(aContentView);

  return NS_OK;
}

nsresult
nsFrameLoader::EnsureMessageManager()
{
  NS_ENSURE_STATE(mOwnerContent);

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mIsTopLevelContent && !OwnerIsBrowserFrame() && !mRemoteFrame) {
    return NS_OK;
  }

  if (mMessageManager) {
    if (ShouldUseRemoteProcess()) {
      mMessageManager->SetCallbackData(mRemoteBrowserShown ? this : nsnull);
    }
    return NS_OK;
  }

  nsIScriptContext* sctx = mOwnerContent->GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(sctx);
  JSContext* cx = sctx->GetNativeContext();
  NS_ENSURE_STATE(cx);

  nsCOMPtr<nsIDOMChromeWindow> chromeWindow =
    do_QueryInterface(GetOwnerDoc()->GetWindow());
  nsCOMPtr<nsIChromeFrameMessageManager> parentManager;
  if (chromeWindow) {
    chromeWindow->GetMessageManager(getter_AddRefs(parentManager));
  }

  if (ShouldUseRemoteProcess()) {
    mMessageManager = new nsFrameMessageManager(true,
                                                nsnull,
                                                SendAsyncMessageToChild,
                                                LoadScript,
                                                mRemoteBrowserShown ? this : nsnull,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx);
  } else {
    mMessageManager = new nsFrameMessageManager(true,
                                                nsnull,
                                                SendAsyncMessageToChild,
                                                LoadScript,
                                                nsnull,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx);
    mChildMessageManager =
      new nsInProcessTabChildGlobal(mDocShell, mOwnerContent, mMessageManager);
    mMessageManager->SetCallbackData(this);
  }
  return NS_OK;
}

nsIDOMEventTarget*
nsFrameLoader::GetTabChildGlobalAsEventTarget()
{
  return static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
}

NS_IMETHODIMP
nsFrameLoader::GetOwnerElement(nsIDOMElement **aElement)
{
  nsCOMPtr<nsIDOMElement> ownerElement = do_QueryInterface(mOwnerContent);
  ownerElement.forget(aElement);
  return NS_OK;
}

void
nsFrameLoader::SetRemoteBrowser(nsITabParent* aTabParent)
{
  MOZ_ASSERT(!mRemoteBrowser);
  MOZ_ASSERT(!mCurrentRemoteFrame);
  mRemoteBrowser = static_cast<TabParent*>(aTabParent);
}

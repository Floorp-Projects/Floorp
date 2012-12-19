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

#include "mozIApplication.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMMozBrowserFrame.h"
#include "nsIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMFile.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDOMApplicationRegistry.h"
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
#include "nsError.h"
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

#include "AppProcessPermissions.h"
#include "ContentParent.h"
#include "TabParent.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "nsIAppsService.h"

#include "jsapi.h"
#include "nsHTMLIFrameElement.h"
#include "nsSandboxFlags.h"

#include "mozilla/dom/StructuredCloneUtils.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildMessageManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "nsFrameLoader::mMessageManager");
  cb.NoteXPCOMChild(static_cast<nsIContentFrameMessageManager*>(tmp->mMessageManager.get()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildMessageManager)
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
  , mDetachedSubdocViews(nullptr)
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
  , mRemoteBrowserInitialized(false)
  , mObservingOwnerContent(false)
  , mCurrentRemoteFrame(nullptr)
  , mRemoteBrowser(nullptr)
  , mRenderMode(RENDER_MODE_DEFAULT)
  , mEventMode(EVENT_MODE_NORMAL_DISPATCH)
{
}

nsFrameLoader*
nsFrameLoader::Create(Element* aOwner, bool aNetworkCreated)
{
  NS_ENSURE_TRUE(aOwner, nullptr);
  nsIDocument* doc = aOwner->OwnerDoc();
  NS_ENSURE_TRUE(!doc->GetDisplayDocument() &&
                 ((!doc->IsLoadedAsData() && aOwner->GetCurrentDoc()) ||
                   doc->IsStaticDocument()),
                 nullptr);

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
  const char *charset = doc_charset.IsEmpty() ? nullptr : doc_charset.get();

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
    mURIToLoad = nullptr;
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

  // Is this an <iframe> with a sandbox attribute or a parent which is
  // sandboxed ?
  nsHTMLIFrameElement* iframe =
    nsHTMLIFrameElement::FromContent(mOwnerContent);

  uint32_t sandboxFlags = 0;

  if (iframe) {
    sandboxFlags = iframe->GetSandboxFlags();

    uint32_t parentSandboxFlags = iframe->OwnerDoc()->GetSandboxFlags();

    if (sandboxFlags || parentSandboxFlags) {
      // The child can only add restrictions, not remove them.
      sandboxFlags |= parentSandboxFlags;

      mDocShell->SetSandboxFlags(sandboxFlags);
    }
  }

  // If this is an <iframe> and it's sandboxed with respect to origin
  // we will set it up with a null principal later in nsDocShell::DoURILoad.
  // We do it there to correctly sandbox content that was loaded into
  // the iframe via other methods than the src attribute.
  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!
  loadInfo->SetOwner(mOwnerContent->NodePrincipal());

  nsCOMPtr<nsIURI> referrer;
  rv = mOwnerContent->NodePrincipal()->GetURI(getter_AddRefs(referrer));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo->SetReferrer(referrer);

  // Default flags:
  int32_t flags = nsIWebNavigation::LOAD_FLAGS_NONE;

  // Flags for browser frame:
  if (OwnerIsBrowserFrame()) {
    flags = nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
            nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_OWNER;
  }

  // Kick off the load...
  bool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = true;
  rv = mDocShell->LoadURI(mURIToLoad, loadInfo, flags, false);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nullptr;
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
  *aDocShell = nullptr;
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
  mDocShell = nullptr;
}

static void
FirePageHideEvent(nsIDocShellTreeItem* aItem,
                  nsIDOMEventTarget* aChromeEventHandler)
{
  nsCOMPtr<nsIDOMDocument> doc = do_GetInterface(aItem);
  nsCOMPtr<nsIDocument> internalDoc = do_QueryInterface(doc);
  NS_ASSERTION(internalDoc, "What happened here?");
  internalDoc->OnPageHide(true, aChromeEventHandler);

  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
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
  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  nsAutoTArray<nsCOMPtr<nsIDocShellTreeItem>, 8> kids;
  kids.AppendElements(childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    aItem->GetChildAt(i, getter_AddRefs(kids[i]));
  }

  for (uint32_t i = 0; i < kids.Length(); ++i) {
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

  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetChildAt(i, getter_AddRefs(item));
    if (aHandler) {
      nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
      shell->SetChromeEventHandler(aHandler);
    }
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

/**
 * Set the type of the treeitem and hook it up to the treeowner.
 * @param aItem the treeitem we're working with
 * @param aTreeOwner the relevant treeowner; might be null
 * @param aParentType the nsIDocShellTreeItem::GetType of our parent docshell
 * @param aParentNode if non-null, the docshell we should be added as a child to
 *
 * @return whether aItem is top-level content
 */
bool
nsFrameLoader::AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                                      nsIDocShellTreeOwner* aOwner,
                                      int32_t aParentType,
                                      nsIDocShellTreeNode* aParentNode)
{
  NS_PRECONDITION(aItem, "Must have docshell treeitem");
  NS_PRECONDITION(mOwnerContent, "Must have owning content");
  
  nsAutoString value;
  bool isContent = false;
  mOwnerContent->GetAttr(kNameSpaceID_None, TypeAttrName(), value);

  // we accept "content" and "content-xxx" values.
  // at time of writing, we expect "xxx" to be "primary" or "targetable", but
  // someday it might be an integer expressing priority or something else.

  isContent = value.LowerCaseEqualsLiteral("content") ||
    StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                     nsCaseInsensitiveStringComparator());

  // Force mozbrowser frames to always be typeContent, even if the
  // mozbrowser interfaces are disabled.
  nsCOMPtr<nsIDOMMozBrowserFrame> mozbrowser =
    do_QueryInterface(mOwnerContent);
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
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
      aOwner->ContentShellAdded(aItem, is_primary, is_targetable, value);
    }
  }

  return retval;
}

static bool
AllDescendantsOfType(nsIDocShellTreeItem* aParentItem, int32_t aType)
{
  int32_t childCount = 0;
  aParentItem->GetChildCount(&childCount);

  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> kid;
    aParentItem->GetChildAt(i, getter_AddRefs(kid));

    int32_t kidType;
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
nsFrameLoader::Show(int32_t marginWidth, int32_t marginHeight,
                    int32_t scrollbarPrefX, int32_t scrollbarPrefY,
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

    mDocShell->SetMarginWidth(marginWidth);
    mDocShell->SetMarginHeight(marginHeight);

    nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mDocShell);
    if (sc) {
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                         scrollbarPrefX);
      sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                         scrollbarPrefY);
    }

    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      // Ensure root scroll frame is reflowed in case scroll preferences or
      // margins have changed
      nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
      if (rootScrollFrame) {
        presShell->FrameNeedsReflow(rootScrollFrame, nsIPresShell::eResize,
                                    NS_FRAME_IS_DIRTY);
      }
      return true;
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
  baseWindow->InitWindow(nullptr, view->GetWidget(), 0, 0,
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
nsFrameLoader::MarginsChanged(uint32_t aMarginWidth,
                              uint32_t aMarginHeight)
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
    if (OwnerIsBrowserOrAppFrame() && os && !mRemoteBrowserInitialized) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                          "remote-browser-frame-shown", NULL);
      mRemoteBrowserInitialized = true;
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
  baseWin->SetParentWidget(nullptr);
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
  // swapping is certainly not safe. If this needs to be changed then
  // the code below needs to be audited as it assumes identical types.
  int32_t ourType = nsIDocShellTreeItem::typeChrome;
  int32_t otherType = nsIDocShellTreeItem::typeChrome;
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
  int32_t ourParentType = nsIDocShellTreeItem::typeContent;
  int32_t otherParentType = nsIDocShellTreeItem::typeContent;
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

  if (ourDocshell->GetIsBrowserElement() !=
      otherDocshell->GetIsBrowserElement() ||
      ourDocshell->GetIsApp() != otherDocshell->GetIsApp()) {
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

  // Restore the correct chrome event handlers.
  ourDocshell->SetChromeEventHandler(otherChromeEventHandler);
  otherDocshell->SetChromeEventHandler(ourChromeEventHandler);
  // Restore the correct treeowners
  // (and also chrome event handlers for content frames only).
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(ourTreeItem, otherOwner,
    ourType == nsIDocShellTreeItem::typeContent ? otherChromeEventHandler : nullptr);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(otherTreeItem, ourOwner,
    ourType == nsIDocShellTreeItem::typeContent ? ourChromeEventHandler : nullptr);

  // Switch the owner content before we start calling AddTreeItemToTreeOwner.
  // Note that we rely on this to deal with setting mObservingOwnerContent to
  // false and calling RemoveMutationObserver as needed.
  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  AddTreeItemToTreeOwner(ourTreeItem, otherOwner, otherParentType, nullptr);
  aOther->AddTreeItemToTreeOwner(otherTreeItem, ourOwner, ourParentType,
                                 nullptr);

  // SetSubDocumentFor nulls out parent documents on the old child doc if a
  // new non-null document is passed in, so just go ahead and remove both
  // kids before reinserting in the parent subdoc maps, to avoid
  // complications.
  ourParentDocument->SetSubDocumentFor(ourContent, nullptr);
  otherParentDocument->SetSubDocumentFor(otherContent, nullptr);
  ourParentDocument->SetSubDocumentFor(ourContent, otherChildDocument);
  otherParentDocument->SetSubDocumentFor(otherContent, ourChildDocument);

  ourWindow->SetFrameElementInternal(otherFrameElement);
  otherWindow->SetFrameElementInternal(ourFrameElement);

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
    mMessageManager->GetParentManager() : nullptr;
  nsFrameMessageManager* otherParentManager = aOther->mMessageManager ?
    aOther->mMessageManager->GetParentManager() : nullptr;
  JSContext* thisCx =
    mMessageManager ? mMessageManager->GetJSContext() : nullptr;
  JSContext* otherCx = 
    aOther->mMessageManager ? aOther->mMessageManager->GetJSContext() : nullptr;
  if (mMessageManager) {
    mMessageManager->RemoveFromParent();
    mMessageManager->SetJSContext(otherCx);
    mMessageManager->SetParentManager(otherParentManager);
    mMessageManager->SetCallback(aOther, false);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->RemoveFromParent();
    aOther->mMessageManager->SetJSContext(thisCx);
    aOther->mMessageManager->SetParentManager(ourParentManager);
    aOther->mMessageManager->SetCallback(this, false);
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
    mRemoteBrowser->SetOwnerElement(nullptr);
    mRemoteBrowser->Destroy();
    mRemoteBrowser = nullptr;
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
    doc->SetSubDocumentFor(mOwnerContent, nullptr);

    SetOwnerContent(nullptr);
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
    win_private->SetFrameElementInternal(nullptr);
  }

  if ((mNeedsAsyncDestroy || !doc ||
       NS_FAILED(doc->FinalizeFrameLoader(this))) && mDocShell) {
    nsCOMPtr<nsIRunnable> event = new nsAsyncDocShellDestroyer(mDocShell);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    NS_DispatchToCurrentThread(event);

    // Let go of our docshell now that the async destroyer holds on to
    // the docshell.

    mDocShell = nullptr;
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
  if (mObservingOwnerContent) {
    mObservingOwnerContent = false;
    mOwnerContent->RemoveMutationObserver(this);
  }
  mOwnerContent = aContent;
  if (RenderFrameParent* rfp = GetCurrentRemoteFrame()) {
    rfp->OwnerContentChanged(aContent);
  }
}

bool
nsFrameLoader::OwnerIsBrowserOrAppFrame()
{
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  return browserFrame ? browserFrame->GetReallyIsBrowserOrApp() : false;
}

bool
nsFrameLoader::OwnerIsAppFrame()
{
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  return browserFrame ? browserFrame->GetReallyIsApp() : false;
}

bool
nsFrameLoader::OwnerIsBrowserFrame()
{
  return OwnerIsBrowserOrAppFrame() && !OwnerIsAppFrame();
}

void
nsFrameLoader::GetOwnerAppManifestURL(nsAString& aOut)
{
  aOut.Truncate();
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->GetAppManifestURL(aOut);
  }
}

already_AddRefed<mozIApplication>
nsFrameLoader::GetOwnApp()
{
  nsAutoString manifest;
  GetOwnerAppManifestURL(manifest);
  if (manifest.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(appsService, nullptr);

  nsCOMPtr<mozIDOMApplication> domApp;
  appsService->GetAppByManifestURL(manifest, getter_AddRefs(domApp));

  nsCOMPtr<mozIApplication> app = do_QueryInterface(domApp);
  MOZ_ASSERT_IF(domApp, app);
  return app.forget();
}

already_AddRefed<mozIApplication>
nsFrameLoader::GetContainingApp()
{
  // See if our owner content's principal has an associated app.
  uint32_t appId = mOwnerContent->NodePrincipal()->GetAppId();
  MOZ_ASSERT(appId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  if (appId == nsIScriptSecurityManager::NO_APP_ID ||
      appId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    return nullptr;
  }

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(appsService, nullptr);

  nsCOMPtr<mozIDOMApplication> domApp;
  appsService->GetAppByLocalId(appId, getter_AddRefs(domApp));
  MOZ_ASSERT(domApp);

  nsCOMPtr<mozIApplication> app = do_QueryInterface(domApp);
  MOZ_ASSERT_IF(domApp, app);
  return app.forget();
}

bool
nsFrameLoader::ShouldUseRemoteProcess()
{
  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS") ||
      Preferences::GetBool("dom.ipc.tabs.disabled", false)) {
    return false;
  }

  // If we're inside a content process, don't use a remote process for this
  // frame; it won't work properly until bug 761935 is fixed.
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return false;
  }

  // If we're an <iframe mozbrowser> and we don't have a "remote" attribute,
  // fall back to the default.
  if (OwnerIsBrowserOrAppFrame() &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::Remote)) {

    return Preferences::GetBool("dom.ipc.browser_frames.oop_by_default", false);
  }

  // Otherwise, we're remote if we have "remote=true" and we're either a
  // browser frame or a XUL element.
  return (OwnerIsBrowserOrAppFrame() ||
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

  int32_t namespaceID = mOwnerContent->GetNameSpaceID();
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

    int32_t parentType;
    parentAsItem->GetItemType(&parentType);

    // XXXbz why is this in content code, exactly?  We should handle
    // this some other way.....  Not sure how yet.
    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    NS_ENSURE_STATE(parentTreeOwner);
    mIsTopLevelContent =
      AddTreeItemToTreeOwner(docShellAsItem, parentTreeOwner, parentType,
                             parentAsNode);

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

  if (OwnerIsAppFrame()) {
    // You can't be both an app and a browser frame.
    MOZ_ASSERT(!OwnerIsBrowserFrame());

    nsCOMPtr<mozIApplication> ownApp = GetOwnApp();
    MOZ_ASSERT(ownApp);
    uint32_t ownAppId = nsIScriptSecurityManager::NO_APP_ID;
    if (ownApp) {
      NS_ENSURE_SUCCESS(ownApp->GetLocalId(&ownAppId), NS_ERROR_FAILURE);
    }

    mDocShell->SetIsApp(ownAppId);
  }

  if (OwnerIsBrowserFrame()) {
    // You can't be both a browser and an app frame.
    MOZ_ASSERT(!OwnerIsAppFrame());

    nsCOMPtr<mozIApplication> containingApp = GetContainingApp();
    uint32_t containingAppId = nsIScriptSecurityManager::NO_APP_ID;
    if (containingApp) {
      NS_ENSURE_SUCCESS(containingApp->GetLocalId(&containingAppId),
                        NS_ERROR_FAILURE);
    }
    mDocShell->SetIsBrowserInsideApp(containingAppId);
  }

  if (OwnerIsBrowserOrAppFrame()) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                          "in-process-browser-or-app-frame-shown", NULL);
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
  
  
  int32_t ourType;
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
  int32_t depth = 0;
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
  int32_t matchCount = 0;
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
    int32_t x = 0;
    int32_t y = 0;

    nsWeakFrame weakFrame(aIFrame);

    baseWindow->GetPositionAndSize(&x, &y, nullptr, nullptr);

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
nsFrameLoader::GetRenderMode(uint32_t* aRenderMode)
{
  *aRenderMode = mRenderMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetRenderMode(uint32_t aRenderMode)
{
  if (aRenderMode == mRenderMode) {
    return NS_OK;
  }

  mRenderMode = aRenderMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetEventMode(uint32_t* aEventMode)
{
  *aEventMode = mEventMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetEventMode(uint32_t aEventMode)
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
    frame->InvalidateFrame();
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
  if (!OwnerIsBrowserOrAppFrame()) {
    int32_t parentType;
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

  uint32_t chromeFlags = 0;
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

  MutableTabContext context;
  nsCOMPtr<mozIApplication> ownApp = GetOwnApp();
  nsCOMPtr<mozIApplication> containingApp = GetContainingApp();
  ScrollingBehavior scrollingBehavior = DEFAULT_SCROLLING;
  if (mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                 nsGkAtoms::mozasyncpanzoom,
                                 nsGkAtoms::_true,
                                 eCaseMatters)) {
    scrollingBehavior = ASYNC_PAN_ZOOM;
  }
  if (ownApp) {
    context.SetTabContextForAppFrame(ownApp, containingApp, scrollingBehavior);
  } else if (OwnerIsBrowserFrame()) {
    // The |else| above is unnecessary; OwnerIsBrowserFrame() implies !ownApp.
    context.SetTabContextForBrowserFrame(containingApp, scrollingBehavior);
  }

  mRemoteBrowser = ContentParent::CreateBrowserOrApp(context);
  if (mRemoteBrowser) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mOwnerContent);
    mRemoteBrowser->SetOwnerElement(element);

    // If we're an app, send the frame element's mozapptype down to the child
    // process.  This ends up in TabChild::GetAppType().
    if (ownApp) {
      nsAutoString appType;
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapptype, appType);
      mRemoteBrowser->SendSetAppType(appType);
    }

    nsCOMPtr<nsIDocShellTreeItem> rootItem;
    parentAsItem->GetRootTreeItem(getter_AddRefs(rootItem));
    nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(rootItem);
    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);
    NS_ABORT_IF_FALSE(rootChromeWin, "How did we not get a chrome window here?");

    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    mRemoteBrowser->SetBrowserDOMWindow(browserDOMWin);

    mChildHost = static_cast<ContentParent*>(mRemoteBrowser->Manager());
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
                                          int32_t aButton,
                                          int32_t aClickCount,
                                          int32_t aModifiers,
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
                                        int32_t aKeyCode,
                                        int32_t aCharCode,
                                        int32_t aModifiers,
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

bool
nsFrameLoader::DoLoadFrameScript(const nsAString& aURL)
{
  mozilla::dom::PBrowserParent* tabParent = GetRemoteBrowser();
  if (tabParent) {
    return tabParent->SendLoadRemoteScript(nsString(aURL));
  }
  nsRefPtr<nsInProcessTabChildGlobal> tabChild =
    static_cast<nsInProcessTabChildGlobal*>(GetTabChildGlobalAsEventTarget());
  if (tabChild) {
    tabChild->LoadFrameScript(aURL);
  }
  return true;
}

class nsAsyncMessageToChild : public nsRunnable
{
public:
  nsAsyncMessageToChild(nsFrameLoader* aFrameLoader,
                        const nsAString& aMessage,
                        const StructuredCloneData& aData)
    : mFrameLoader(aFrameLoader), mMessage(aMessage)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  NS_IMETHOD Run()
  {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mFrameLoader->mChildMessageManager.get());
    if (tabChild && tabChild->GetInnerManager()) {
      nsFrameScriptCx cx(static_cast<nsIDOMEventTarget*>(tabChild), tabChild);

      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      nsRefPtr<nsFrameMessageManager> mm = tabChild->GetInnerManager();
      mm->ReceiveMessage(static_cast<nsIDOMEventTarget*>(tabChild), mMessage,
                         false, &data, nullptr, nullptr, nullptr);
    }
    return NS_OK;
  }
  nsRefPtr<nsFrameLoader> mFrameLoader;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
};

bool
nsFrameLoader::DoSendAsyncMessage(const nsAString& aMessage,
                                  const StructuredCloneData& aData)
{
  PBrowserParent* tabParent = GetRemoteBrowser();
  if (tabParent) {
    ClonedMessageData data;

    SerializedStructuredCloneBuffer& buffer = data.data();
    buffer.data = aData.mData;
    buffer.dataLength = aData.mDataLength;

    const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
    if (!blobs.IsEmpty()) {
      InfallibleTArray<PBlobParent*>& blobParents = data.blobsParent();

      uint32_t length = blobs.Length();
      blobParents.SetCapacity(length);

      ContentParent* cp = static_cast<ContentParent*>(tabParent->Manager());

      for (uint32_t i = 0; i < length; ++i) {
        BlobParent* blobParent = cp->GetOrCreateActorForBlob(blobs[i]);
        if (!blobParent) {
          return false;
        }

        blobParents.AppendElement(blobParent);
      }
    }

    return tabParent->SendAsyncMessage(nsString(aMessage), data);
  }

  if (mChildMessageManager) {
    nsRefPtr<nsIRunnable> ev = new nsAsyncMessageToChild(this, aMessage, aData);
    NS_DispatchToCurrentThread(ev);
    return true;
  }

  // We don't have any targets to send our asynchronous message to.
  return false;
}

bool
nsFrameLoader::CheckPermission(const nsAString& aPermission)
{
  return AssertAppProcessPermission(GetRemoteBrowser(),
                                    NS_ConvertUTF16toUTF8(aPermission).get());
}

NS_IMETHODIMP
nsFrameLoader::GetMessageManager(nsIMessageSender** aManager)
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
                                 uint32_t* aLength,
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
    *aResult = nullptr;
    *aLength = 0;
    return NS_OK;
  }

  nsIContentView** result = reinterpret_cast<nsIContentView**>(
    NS_Alloc(ids.Length() * sizeof(nsIContentView*)));

  for (uint32_t i = 0; i < ids.Length(); i++) {
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
    *aContentView = nullptr;
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

  if (!mIsTopLevelContent && !OwnerIsBrowserOrAppFrame() && !mRemoteFrame) {
    return NS_OK;
  }

  if (mMessageManager) {
    if (ShouldUseRemoteProcess()) {
      mMessageManager->SetCallback(mRemoteBrowserShown ? this : nullptr);
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
  nsCOMPtr<nsIMessageBroadcaster> parentManager;
  if (chromeWindow) {
    chromeWindow->GetMessageManager(getter_AddRefs(parentManager));
  }

  if (ShouldUseRemoteProcess()) {
    mMessageManager = new nsFrameMessageManager(mRemoteBrowserShown ? this : nullptr,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx,
                                                MM_CHROME);
  } else {
    mMessageManager = new nsFrameMessageManager(nullptr,
                                                static_cast<nsFrameMessageManager*>(parentManager.get()),
                                                cx,
                                                MM_CHROME);

    mChildMessageManager =
      new nsInProcessTabChildGlobal(mDocShell, mOwnerContent, mMessageManager);
    // Force pending frame scripts to be loaded.
    mMessageManager->SetCallback(this);
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
  mRemoteFrame = true;
  mRemoteBrowser = static_cast<TabParent*>(aTabParent);

  ShowRemoteFrame(nsIntSize(0, 0));
}

void
nsFrameLoader::SetDetachedSubdocView(nsIView* aDetachedViews,
                                     nsIDocument* aContainerDoc)
{
  mDetachedSubdocViews = aDetachedViews;
  mContainerDocWhileDetached = aContainerDoc;
}

nsIView*
nsFrameLoader::GetDetachedSubdocView(nsIDocument** aContainerDoc) const
{
  NS_IF_ADDREF(*aContainerDoc = mContainerDocWhileDetached);
  return mDetachedSubdocViews;
}

/* virtual */ void
nsFrameLoader::AttributeChanged(nsIDocument* aDocument,
                                mozilla::dom::Element* aElement,
                                int32_t      aNameSpaceID,
                                nsIAtom*     aAttribute,
                                int32_t      aModType)
{
  MOZ_ASSERT(mObservingOwnerContent);
  // TODO: Implement ContentShellAdded for remote browsers (bug 658304)
  MOZ_ASSERT(!mRemoteBrowser);

  if (aNameSpaceID != kNameSpaceID_None || aAttribute != TypeAttrName()) {
    return;
  }

  if (aElement != mOwnerContent) {
    return;
  }

  // Note: This logic duplicates a lot of logic in
  // MaybeCreateDocshell.  We should fix that.

  // Notify our enclosing chrome that our type has changed.  We only do this
  // if our parent is chrome, since in all other cases we're random content
  // subframes and the treeowner shouldn't worry about us.

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (!docShellAsItem) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  docShellAsItem->GetParent(getter_AddRefs(parentItem));
  if (!parentItem) {
    return;
  }

  int32_t parentType;
  parentItem->GetItemType(&parentType);

  if (parentType != nsIDocShellTreeItem::typeChrome) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  if (!parentTreeOwner) {
    return;
  }

  nsAutoString value;
  aElement->GetAttr(kNameSpaceID_None, TypeAttrName(), value);

  bool is_primary = value.LowerCaseEqualsLiteral("content-primary");

#ifdef MOZ_XUL
  // when a content panel is no longer primary, hide any open popups it may have
  if (!is_primary) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->HidePopupsInDocShell(docShellAsItem);
  }
#endif

  parentTreeOwner->ContentShellRemoved(docShellAsItem);
  if (value.LowerCaseEqualsLiteral("content") ||
      StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                       nsCaseInsensitiveStringComparator())) {
    bool is_targetable = is_primary ||
      value.LowerCaseEqualsLiteral("content-targetable");

    parentTreeOwner->ContentShellAdded(docShellAsItem, is_primary,
                                       is_targetable, value);
  }
}

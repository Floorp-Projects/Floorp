/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#include "nsFrameLoader.h"

#include "base/basictypes.h"

#include "prenv.h"

#include "nsDocShell.h"
#include "nsIContentInlines.h"
#include "nsIContentViewer.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsDocShellLoadState.h"
#include "nsIBaseWindow.h"
#include "nsIBrowser.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsFrameLoaderOwner.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsError.h"
#include "nsIAppWindow.h"
#include "nsIMozBrowserFrame.h"
#include "nsIScriptError.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsPIWindowRoot.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsView.h"
#include "nsBaseWidget.h"
#include "nsQueryObject.h"
#include "ReferrerInfo.h"
#include "nsIOpenWindowInfo.h"
#include "nsISHistory.h"
#include "nsIURI.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"
#include "nsFocusManager.h"
#include "nsIINIParser.h"
#include "nsAppRunner.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

#include "nsThreadUtils.h"

#include "nsIDOMChromeWindow.h"
#include "InProcessBrowserChildMessageManager.h"

#include "Layers.h"

#include "ContentParent.h"
#include "BrowserParent.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ExpandedPrincipal.h"
#include "mozilla/FlushType.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FrameCrashedEvent.h"
#include "mozilla/dom/FrameLoaderBinding.h"
#include "mozilla/dom/MozFrameLoaderOwnerBinding.h"
#include "mozilla/dom/PBrowser.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/SessionStoreChangeListener.h"
#include "mozilla/dom/SessionStoreListener.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/ProfilerLabels.h"
#include "nsGenericHTMLFrameElement.h"

#include "jsapi.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "nsSandboxFlags.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/dom/CustomEvent.h"

#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/WebBrowserPersistLocalDocument.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/ChildSHistory.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowserBridgeHost.h"
#include "mozilla/dom/BrowsingContextGroup.h"

#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "mozilla/ipc/BackgroundUtils.h"

#include "mozilla/dom/HTMLBodyElement.h"

#include "mozilla/ContentPrincipal.h"

#ifdef MOZ_XUL
#  include "nsXULPopupManager.h"
#endif

#ifdef NS_PRINTING
#  include "mozilla/embedding/printingui/PrintingParent.h"
#  include "nsIWebBrowserPrint.h"
#endif

#if defined(MOZ_TELEMETRY_REPORTING)
#  include "mozilla/Telemetry.h"
#endif  // defined(MOZ_TELEMETRY_REPORTING)

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using ViewID = ScrollableLayerGuid::ViewID;

using PrintPreviewResolver = std::function<void(const PrintPreviewResultInfo&)>;

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsFrameLoader, mPendingBrowsingContext,
                                      mMessageManager, mChildMessageManager,
                                      mRemoteBrowser,
                                      mSessionStoreChangeListener)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_CONCRETE(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsFrameLoader::nsFrameLoader(Element* aOwner, BrowsingContext* aBrowsingContext,
                             bool aIsRemoteFrame, bool aNetworkCreated)
    : mPendingBrowsingContext(aBrowsingContext),
      mOwnerContent(aOwner),
      mDetachedSubdocFrame(nullptr),
      mPendingSwitchID(0),
      mChildID(0),
      mRemoteType(NOT_REMOTE_TYPE),
      mInitialized(false),
      mDepthTooGreat(false),
      mIsTopLevelContent(false),
      mDestroyCalled(false),
      mNeedsAsyncDestroy(false),
      mInSwap(false),
      mInShow(false),
      mHideCalled(false),
      mNetworkCreated(aNetworkCreated),
      mLoadingOriginalSrc(false),
      mRemoteBrowserShown(false),
      mIsRemoteFrame(aIsRemoteFrame),
      mWillChangeProcess(false),
      mObservingOwnerContent(false),
      mTabProcessCrashFired(false) {
  nsCOMPtr<nsFrameLoaderOwner> owner = do_QueryInterface(aOwner);
  owner->AttachFrameLoader(this);
}

nsFrameLoader::~nsFrameLoader() {
  if (mMessageManager) {
    mMessageManager->Disconnect();
  }

  MOZ_ASSERT(!mOwnerContent);
  MOZ_RELEASE_ASSERT(mDestroyCalled);
}

static nsAtom* TypeAttrName(Element* aOwnerContent) {
  return aOwnerContent->IsXULElement() ? nsGkAtoms::type
                                       : nsGkAtoms::mozframetype;
}

static void GetFrameName(Element* aOwnerContent, nsAString& aFrameName) {
  int32_t namespaceID = aOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML && !aOwnerContent->IsInHTMLDocument()) {
    aOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, aFrameName);
  } else {
    aOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, aFrameName);
    // XXX if no NAME then use ID, after a transition period this will be
    // changed so that XUL only uses ID too (bug 254284).
    if (aFrameName.IsEmpty() && namespaceID == kNameSpaceID_XUL) {
      aOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, aFrameName);
    }
  }
}

// If this method returns true, the nsFrameLoader will act as a boundary, as is
// the case for <iframe mozbrowser> and <browser type="content"> elements.
//
// # Historical Notes  (10 April 2019)
//
// In the past, this boundary was defined by the "typeContent" and "typeChrome"
// nsIDocShellTreeItem types. There was only ever a single split in the tree,
// and it occurred at the boundary between these two types of docshells. When
// <iframe mozbrowser> was introduced, it was given special casing to make it
// act like a second boundary, without having to change the existing code.
//
// The about:addons page, which is loaded within a content browser, then added a
// remote <browser type="content" remote="true"> element. When remote, this
// would also act as a mechanism for creating a disjoint tree, due to the
// process keeping the embedder and embedee separate.
//
// However, when initial out-of-process iframe support was implemented, this
// codepath became a risk, as it could've caused the oop iframe remote
// WindowProxy code to be activated for the addons page. This was fixed by
// extendng the isolation logic previously reserved to <iframe mozbrowser> to
// also cover <browser> elements with the explicit `remote` property loaded in
// content.
//
// To keep these boundaries clear, and allow them to work in a cross-process
// manner, they are no longer handled by typeContent and typeChrome. Instead,
// the actual BrowsingContext tree is broken at these edges.
static bool IsTopContent(BrowsingContext* aParent, Element* aOwner) {
  if (XRE_IsContentProcess()) {
    return false;
  }

  // If we have a (deprecated) mozbrowser element, we want to start a new
  // BrowsingContext tree regardless of whether the parent is chrome or content.
  nsCOMPtr<nsIMozBrowserFrame> mozbrowser = aOwner->GetAsMozBrowserFrame();
  if (mozbrowser && mozbrowser->GetReallyIsBrowser()) {
    return true;
  }

  if (aParent->IsContent()) {
    // If we're already in content, we may still want to create a new
    // BrowsingContext tree if our element is a xul browser element with a
    // `remote="true"` marker.
    return aOwner->IsXULElement() &&
           aOwner->AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                               nsGkAtoms::_true, eCaseMatters);
  }

  // If we're in a chrome context, we want to start a new tree if we are an
  // element with a `type="content"` marker.
  return aOwner->AttrValueIs(kNameSpaceID_None, TypeAttrName(aOwner),
                             nsGkAtoms::content, eIgnoreCase);
}

static already_AddRefed<BrowsingContext> CreateBrowsingContext(
    Element* aOwner, nsIOpenWindowInfo* aOpenWindowInfo,
    BrowsingContextGroup* aSpecificGroup, bool aNetworkCreated = false) {
  MOZ_ASSERT(!aOpenWindowInfo || !aSpecificGroup,
             "Only one of SpecificGroup and OpenWindowInfo may be provided!");

  // If we've got a pending BrowserParent from the content process, use the
  // BrowsingContext which was created for it.
  if (aOpenWindowInfo && aOpenWindowInfo->GetNextRemoteBrowser()) {
    MOZ_ASSERT(XRE_IsParentProcess());
    return do_AddRef(
        aOpenWindowInfo->GetNextRemoteBrowser()->GetBrowsingContext());
  }

  RefPtr<BrowsingContext> opener;
  if (aOpenWindowInfo && !aOpenWindowInfo->GetForceNoOpener()) {
    opener = aOpenWindowInfo->GetParent();
    if (opener) {
      // Must create BrowsingContext with opener in-process.
      MOZ_ASSERT(opener->IsInProcess());

      // This can only happen when the opener was closed from a nested event
      // loop in the window provider code, and only when the open was triggered
      // by a non-e10s tab, and the new tab is being opened in a new browser
      // window. Since it is a corner case among corner cases, and the opener
      // window will appear to be null to consumers after it is discarded
      // anyway, just drop the opener entirely.
      if (opener->IsDiscarded()) {
        NS_WARNING(
            "Opener was closed from a nested event loop in the parent process. "
            "Please fix this.");
        opener = nullptr;
      }
    }
  }

  RefPtr<nsGlobalWindowInner> parentInner =
      nsGlobalWindowInner::Cast(aOwner->OwnerDoc()->GetInnerWindow());
  if (NS_WARN_IF(!parentInner) || parentInner->IsDying()) {
    return nullptr;
  }

  BrowsingContext* parentBC = parentInner->GetBrowsingContext();
  if (NS_WARN_IF(!parentBC) || parentBC->IsDiscarded()) {
    return nullptr;
  }

  // Determine the frame name for the new browsing context.
  nsAutoString frameName;
  GetFrameName(aOwner, frameName);

  // Create our BrowsingContext without immediately attaching it. It's possible
  // that no DocShell or remote browser will ever be created for this
  // FrameLoader, particularly if the document that we were created for is not
  // currently active. And in that latter case, if we try to attach our BC now,
  // it will wind up attached as a child of the currently active inner window
  // for the BrowsingContext, and cause no end of trouble.
  if (IsTopContent(parentBC, aOwner)) {
    // Create toplevel context without a parent & as Type::Content.
    return BrowsingContext::CreateDetached(
        nullptr, opener, aSpecificGroup, frameName,
        BrowsingContext::Type::Content, false);
  }

  MOZ_ASSERT(!aOpenWindowInfo,
             "Can't have openWindowInfo for non-toplevel context");

  MOZ_ASSERT(!aSpecificGroup,
             "Can't force BrowsingContextGroup for non-toplevel context");
  return BrowsingContext::CreateDetached(parentInner, nullptr, nullptr,
                                         frameName, parentBC->GetType(), false,
                                         !aNetworkCreated);
}

static bool InitialLoadIsRemote(Element* aOwner) {
  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS") ||
      Preferences::GetBool("dom.ipc.tabs.disabled", false)) {
    return false;
  }

  // The initial load in an content process iframe should never be made remote.
  // Content process iframes always become remote due to navigation.
  if (XRE_IsContentProcess()) {
    return false;
  }

  // If we're an <iframe mozbrowser> and we don't have a "remote" attribute,
  // fall back to the default.
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(aOwner);
  bool isMozBrowserFrame = browserFrame && browserFrame->GetReallyIsBrowser();
  if (isMozBrowserFrame &&
      !aOwner->HasAttr(kNameSpaceID_None, nsGkAtoms::remote)) {
    return Preferences::GetBool("dom.ipc.browser_frames.oop_by_default", false);
  }

  // Otherwise, we're remote if we have "remote=true" and we're either a
  // browser frame or a XUL element.
  return (isMozBrowserFrame || aOwner->GetNameSpaceID() == kNameSpaceID_XUL) &&
         aOwner->AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                             nsGkAtoms::_true, eCaseMatters);
}

static already_AddRefed<BrowsingContextGroup> InitialBrowsingContextGroup(
    Element* aOwner) {
  nsAutoString attrString;
  if (aOwner->GetNameSpaceID() != kNameSpaceID_XUL ||
      !aOwner->GetAttr(nsGkAtoms::initialBrowsingContextGroupId, attrString)) {
    return nullptr;
  }

  // It's OK to read the attribute using a signed 64-bit integer parse, as an ID
  // generated using `nsContentUtils::GenerateProcessSpecificId` (like BCG IDs)
  // will only ever use 53 bits of precision, so it can be round-tripped through
  // a JS number.
  nsresult rv = NS_OK;
  int64_t signedGroupId = attrString.ToInteger64(&rv, 10);
  if (NS_FAILED(rv) || signedGroupId <= 0) {
    MOZ_DIAGNOSTIC_ASSERT(
        false, "we intended to have a particular id, but failed to parse it!");
    return nullptr;
  }

  return BrowsingContextGroup::GetOrCreate(uint64_t(signedGroupId));
}

already_AddRefed<nsFrameLoader> nsFrameLoader::Create(
    Element* aOwner, bool aNetworkCreated, nsIOpenWindowInfo* aOpenWindowInfo) {
  NS_ENSURE_TRUE(aOwner, nullptr);
  Document* doc = aOwner->OwnerDoc();

  // We never create nsFrameLoaders for elements in resource documents.
  //
  // We never create nsFrameLoaders for elements in data documents, unless the
  // document is a static document.
  // Static documents are an exception because any sub-documents need an
  // nsFrameLoader to keep the relevant docShell alive, even though the
  // nsFrameLoader isn't used to load anything (the sub-document is created by
  // the static clone process).
  //
  // We never create nsFrameLoaders for elements that are not
  // in-composed-document, unless the element belongs to a static document.
  // Static documents are an exception because this method is called at a point
  // in the static clone process before aOwner has been inserted into its
  // document.  For other types of documents this wouldn't be a problem since
  // we'd create the nsFrameLoader as necessary after aOwner is inserted into a
  // document, but the mechanisms that take care of that don't apply for static
  // documents so we need to create the nsFrameLoader now. (This isn't wasteful
  // since for a static document we know aOwner will end up in a document and
  // the nsFrameLoader will be used for its docShell.)
  //
  NS_ENSURE_TRUE(!doc->IsResourceDoc() &&
                     ((!doc->IsLoadedAsData() && aOwner->IsInComposedDoc()) ||
                      doc->IsStaticDocument()),
                 nullptr);

  RefPtr<BrowsingContextGroup> group = InitialBrowsingContextGroup(aOwner);
  RefPtr<BrowsingContext> context =
      CreateBrowsingContext(aOwner, aOpenWindowInfo, group, aNetworkCreated);
  NS_ENSURE_TRUE(context, nullptr);

  if (XRE_IsParentProcess() && aOpenWindowInfo) {
    MOZ_ASSERT(context->IsTopContent());
    if (RefPtr<BrowsingContext> crossGroupOpener =
            aOpenWindowInfo->GetParent()) {
      context->Canonical()->SetCrossGroupOpenerId(crossGroupOpener->Id());
    }
  }

  bool isRemoteFrame = InitialLoadIsRemote(aOwner);
  RefPtr<nsFrameLoader> fl =
      new nsFrameLoader(aOwner, context, isRemoteFrame, aNetworkCreated);
  fl->mOpenWindowInfo = aOpenWindowInfo;

  // If this is a toplevel initial remote frame, we're looking at a browser
  // loaded in the parent process. Pull the remote type attribute off of the
  // <browser> element to determine which remote type it should be loaded in, or
  // use `DEFAULT_REMOTE_TYPE` if we can't tell.
  if (isRemoteFrame) {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsAutoString remoteType;
    if (aOwner->GetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, remoteType) &&
        !remoteType.IsEmpty()) {
      CopyUTF16toUTF8(remoteType, fl->mRemoteType);
    } else {
      fl->mRemoteType = DEFAULT_REMOTE_TYPE;
    }
  }
  return fl.forget();
}

/* static */
already_AddRefed<nsFrameLoader> nsFrameLoader::Recreate(
    mozilla::dom::Element* aOwner, BrowsingContext* aContext,
    BrowsingContextGroup* aSpecificGroup,
    const NavigationIsolationOptions& aRemotenessOptions, bool aIsRemote,
    bool aNetworkCreated, bool aPreserveContext) {
  NS_ENSURE_TRUE(aOwner, nullptr);

#ifdef DEBUG
  // This version of Create is only called for Remoteness updates, so we can
  // assume we need a FrameLoader here and skip the check in the other Create.
  Document* doc = aOwner->OwnerDoc();
  MOZ_ASSERT(!doc->IsResourceDoc());
  MOZ_ASSERT((!doc->IsLoadedAsData() && aOwner->IsInComposedDoc()) ||
             doc->IsStaticDocument());
#endif

  RefPtr<BrowsingContext> context = aContext;
  if (!context || !aPreserveContext) {
    context = CreateBrowsingContext(aOwner, /* openWindowInfo */ nullptr,
                                    aSpecificGroup);
    if (aContext) {
      MOZ_ASSERT(
          XRE_IsParentProcess(),
          "Recreating browing contexts only supported in the parent process");
      aContext->Canonical()->ReplacedBy(context->Canonical(),
                                        aRemotenessOptions);
    }
  }
  NS_ENSURE_TRUE(context, nullptr);

  RefPtr<nsFrameLoader> fl =
      new nsFrameLoader(aOwner, context, aIsRemote, aNetworkCreated);
  return fl.forget();
}

void nsFrameLoader::LoadFrame(bool aOriginalSrc) {
  if (NS_WARN_IF(!mOwnerContent)) {
    return;
  }

  nsAutoString src;
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIContentSecurityPolicy> csp;

  bool isSrcdoc = mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
                  mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (isSrcdoc) {
    src.AssignLiteral("about:srcdoc");
    principal = mOwnerContent->NodePrincipal();
    csp = mOwnerContent->GetCsp();
  } else {
    GetURL(src, getter_AddRefs(principal), getter_AddRefs(csp));

    src.Trim(" \t\n\r");

    if (src.IsEmpty()) {
      // If the frame is a XUL element and has the attribute 'nodefaultsrc=true'
      // then we will not use 'about:blank' as fallback but return early without
      // starting a load if no 'src' attribute is given (or it's empty).
      if (mOwnerContent->IsXULElement() &&
          mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::nodefaultsrc,
                                     nsGkAtoms::_true, eCaseMatters)) {
        return;
      }
      src.AssignLiteral("about:blank");
      principal = mOwnerContent->NodePrincipal();
      csp = mOwnerContent->GetCsp();
    }
  }

  Document* doc = mOwnerContent->OwnerDoc();
  if (doc->IsStaticDocument()) {
    return;
  }

  nsIURI* base_uri = mOwnerContent->GetBaseURI();
  auto encoding = doc->GetDocumentCharacterSet();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), src, encoding, base_uri);

  // If the URI was malformed, try to recover by loading about:blank.
  if (rv == NS_ERROR_MALFORMED_URI) {
    rv = NS_NewURI(getter_AddRefs(uri), u"about:blank"_ns, encoding, base_uri);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = LoadURI(uri, principal, csp, aOriginalSrc);
  }

  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }
}

void nsFrameLoader::ConfigRemoteProcess(const nsACString& aRemoteType,
                                        ContentParent* aContentParent) {
  MOZ_DIAGNOSTIC_ASSERT(IsRemoteFrame(), "Must be a remote frame");
  MOZ_DIAGNOSTIC_ASSERT(!mRemoteBrowser, "Must not have a browser yet");
  MOZ_DIAGNOSTIC_ASSERT_IF(aContentParent,
                           aContentParent->GetRemoteType() == aRemoteType);

  mRemoteType = aRemoteType;
  mChildID = aContentParent ? aContentParent->ChildID() : 0;
}

void nsFrameLoader::FireErrorEvent() {
  if (!mOwnerContent) {
    return;
  }
  RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
      new LoadBlockingAsyncEventDispatcher(
          mOwnerContent, u"error"_ns, CanBubble::eNo, ChromeOnlyDispatch::eNo);
  loadBlockingAsyncDispatcher->PostDOMEvent();
}

nsresult nsFrameLoader::LoadURI(nsIURI* aURI,
                                nsIPrincipal* aTriggeringPrincipal,
                                nsIContentSecurityPolicy* aCsp,
                                bool aOriginalSrc) {
  if (!aURI) return NS_ERROR_INVALID_POINTER;
  NS_ENSURE_STATE(!mDestroyCalled && mOwnerContent);
  MOZ_ASSERT(
      aTriggeringPrincipal,
      "Must have an explicit triggeringPrincipal to nsFrameLoader::LoadURI.");

  mLoadingOriginalSrc = aOriginalSrc;

  nsCOMPtr<Document> doc = mOwnerContent->OwnerDoc();

  nsresult rv;
  rv = CheckURILoad(aURI, aTriggeringPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  mURIToLoad = aURI;
  mTriggeringPrincipal = aTriggeringPrincipal;
  mCsp = aCsp;
  rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mURIToLoad = nullptr;
    mTriggeringPrincipal = nullptr;
    mCsp = nullptr;
  }
  return rv;
}

void nsFrameLoader::ResumeLoad(uint64_t aPendingSwitchID) {
  Document* doc = mOwnerContent->OwnerDoc();
  if (doc->IsStaticDocument()) {
    // Static doc shouldn't load sub-documents.
    return;
  }

  if (NS_WARN_IF(mDestroyCalled || !mOwnerContent)) {
    FireErrorEvent();
    return;
  }

  mLoadingOriginalSrc = false;
  mURIToLoad = nullptr;
  mPendingSwitchID = aPendingSwitchID;
  mTriggeringPrincipal = mOwnerContent->NodePrincipal();
  mCsp = mOwnerContent->GetCsp();

  nsresult rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mPendingSwitchID = 0;
    mTriggeringPrincipal = nullptr;
    mCsp = nullptr;

    FireErrorEvent();
  }
}

nsresult nsFrameLoader::ReallyStartLoading() {
  nsresult rv = ReallyStartLoadingInternal();
  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }

  return rv;
}

nsresult nsFrameLoader::ReallyStartLoadingInternal() {
  NS_ENSURE_STATE((mURIToLoad || mPendingSwitchID) && mOwnerContent &&
                  mOwnerContent->IsInComposedDoc());

  AUTO_PROFILER_LABEL("nsFrameLoader::ReallyStartLoadingInternal", OTHER);
  RefPtr<nsDocShellLoadState> loadState;
  if (!mPendingSwitchID) {
    loadState = new nsDocShellLoadState(mURIToLoad);
    loadState->SetOriginalFrameSrc(mLoadingOriginalSrc);

    // The triggering principal could be null if the frame is loaded other
    // than the src attribute, for example, the frame is sandboxed. In that
    // case we use the principal of the owner content, which is needed to
    // prevent XSS attaches on documents loaded in subframes.
    if (mTriggeringPrincipal) {
      loadState->SetTriggeringPrincipal(mTriggeringPrincipal);
    } else {
      loadState->SetTriggeringPrincipal(mOwnerContent->NodePrincipal());
    }

    // If we have an explicit CSP, we set it. If not, we only query it from
    // the document in case there was no explicit triggeringPrincipal.
    // Otherwise it's possible that the original triggeringPrincipal did not
    // have a CSP which causes the CSP on the Principal and explicit CSP
    // to be out of sync.
    if (mCsp) {
      loadState->SetCsp(mCsp);
    } else if (!mTriggeringPrincipal) {
      nsCOMPtr<nsIContentSecurityPolicy> csp = mOwnerContent->GetCsp();
      loadState->SetCsp(csp);
    }

    nsAutoString srcdoc;
    bool isSrcdoc =
        mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
        mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::srcdoc, srcdoc);

    if (isSrcdoc) {
      loadState->SetSrcdocData(srcdoc);
      loadState->SetBaseURI(mOwnerContent->GetBaseURI());
    }

    auto referrerInfo = MakeRefPtr<ReferrerInfo>(*mOwnerContent);
    loadState->SetReferrerInfo(referrerInfo);

    loadState->SetIsFromProcessingFrameAttributes();

    // Default flags:
    int32_t flags = nsIWebNavigation::LOAD_FLAGS_NONE;

    // Flags for browser frame:
    if (OwnerIsMozBrowserFrame()) {
      flags = nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
              nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
    }
    loadState->SetLoadFlags(flags);

    loadState->SetFirstParty(false);

    // If we're loading the default about:blank document in a <browser> element,
    // prevent the load from causing a process switch by explicitly overriding
    // remote type selection.
    if (mPendingBrowsingContext->IsTopContent() &&
        mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
        NS_IsAboutBlank(mURIToLoad) &&
        loadState->TriggeringPrincipal()->IsSystemPrincipal()) {
      loadState->SetRemoteTypeOverride(mRemoteType);
    }
  }

  if (IsRemoteFrame()) {
    if (!EnsureRemoteBrowser()) {
      NS_WARNING("Couldn't create child process for iframe.");
      return NS_ERROR_FAILURE;
    }

    if (mPendingSwitchID) {
      mRemoteBrowser->ResumeLoad(mPendingSwitchID);
      mPendingSwitchID = 0;
    } else {
      mRemoteBrowser->LoadURL(loadState);
    }

    if (!mRemoteBrowserShown) {
      // This can fail if it's too early to show the frame, we will retry later.
      Unused << ShowRemoteFrame(ScreenIntSize(0, 0));
    }

    return NS_OK;
  }

  if (GetDocShell()) {
    // If we already have a docshell, ensure that the docshell's storage access
    // flag is cleared.
    GetDocShell()->MaybeClearStorageAccessFlag();
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  MOZ_ASSERT(GetDocShell(),
             "MaybeCreateDocShell succeeded with a null docShell");

  // If we have a pending switch, just resume our load.
  if (mPendingSwitchID) {
    bool tmpState = mNeedsAsyncDestroy;
    mNeedsAsyncDestroy = true;
    rv = GetDocShell()->ResumeRedirectedLoad(mPendingSwitchID, -1);
    mNeedsAsyncDestroy = tmpState;
    mPendingSwitchID = 0;
    return rv;
  }

  // Just to be safe, recheck uri.
  rv = CheckURILoad(mURIToLoad, mTriggeringPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoadingOriginalSrc = false;

  // Kick off the load...
  bool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = true;

  rv = GetDocShell()->LoadURI(loadState, false);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nullptr;
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsFrameLoader::CheckURILoad(nsIURI* aURI,
                                     nsIPrincipal* aTriggeringPrincipal) {
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
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();

  // Get our principal
  nsIPrincipal* principal =
      (aTriggeringPrincipal ? aTriggeringPrincipal
                            : mOwnerContent->NodePrincipal());

  // Check if we are allowed to load absURL
  nsresult rv = secMan->CheckLoadURIWithPrincipal(
      principal, aURI, nsIScriptSecurityManager::STANDARD,
      mOwnerContent->OwnerDoc()->InnerWindowID());
  if (NS_FAILED(rv)) {
    return rv;  // We're not
  }

  // Bail out if this is an infinite recursion scenario
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  return CheckForRecursiveLoad(aURI);
}

nsDocShell* nsFrameLoader::GetDocShell(ErrorResult& aRv) {
  if (IsRemoteFrame()) {
    return nullptr;
  }

  // If we have an owner, make sure we have a docshell and return
  // that. If not, we're most likely in the middle of being torn down,
  // then we just return null.
  if (mOwnerContent) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
    MOZ_ASSERT(GetDocShell(),
               "MaybeCreateDocShell succeeded, but null docShell");
  }

  return GetDocShell();
}

static void SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
    nsIDocShellTreeItem* aItem, nsIDocShellTreeOwner* aOwner,
    EventTarget* aHandler) {
  MOZ_ASSERT(aItem, "Must have item");

  aItem->SetTreeOwner(aOwner);

  int32_t childCount = 0;
  aItem->GetInProcessChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetInProcessChildAt(i, getter_AddRefs(item));
    if (aHandler) {
      nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
      shell->SetChromeEventHandler(aHandler);
    }
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
static bool CheckDocShellType(mozilla::dom::Element* aOwnerContent,
                              nsIDocShellTreeItem* aDocShell, nsAtom* aAtom) {
  bool isContent = aOwnerContent->AttrValueIs(kNameSpaceID_None, aAtom,
                                              nsGkAtoms::content, eIgnoreCase);

  if (!isContent) {
    nsCOMPtr<nsIMozBrowserFrame> mozbrowser =
        aOwnerContent->GetAsMozBrowserFrame();
    if (mozbrowser) {
      mozbrowser->GetMozbrowser(&isContent);
    }
  }

  if (isContent) {
    return aDocShell->ItemType() == nsIDocShellTreeItem::typeContent;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  aDocShell->GetInProcessParent(getter_AddRefs(parent));

  return parent && parent->ItemType() == aDocShell->ItemType();
}
#endif  // defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)

/**
 * Hook up a given TreeItem to its tree owner. aItem's type must have already
 * been set, and it should already be part of the DocShellTree.
 * @param aItem the treeitem we're working with
 * @param aTreeOwner the relevant treeowner; might be null
 */
void nsFrameLoader::AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                                           nsIDocShellTreeOwner* aOwner) {
  MOZ_ASSERT(aItem, "Must have docshell treeitem");
  MOZ_ASSERT(mOwnerContent, "Must have owning content");

  MOZ_DIAGNOSTIC_ASSERT(
      CheckDocShellType(mOwnerContent, aItem, TypeAttrName(mOwnerContent)),
      "Correct ItemType should be set when creating BrowsingContext");

  if (mIsTopLevelContent) {
    bool is_primary = mOwnerContent->AttrValueIs(
        kNameSpaceID_None, nsGkAtoms::primary, nsGkAtoms::_true, eIgnoreCase);
    if (aOwner) {
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
      aOwner->ContentShellAdded(aItem, is_primary);
    }
  }
}

static bool AllDescendantsOfType(BrowsingContext* aParent,
                                 BrowsingContext::Type aType) {
  for (auto& child : aParent->Children()) {
    if (child->GetType() != aType || !AllDescendantsOfType(child, aType)) {
      return false;
    }
  }

  return true;
}

void nsFrameLoader::MaybeShowFrame() {
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    nsSubDocumentFrame* subDocFrame = do_QueryFrame(frame);
    if (subDocFrame) {
      subDocFrame->MaybeShowViewer();
    }
  }
}

static ScrollbarPreference GetScrollbarPreference(const Element* aOwner) {
  if (!aOwner) {
    return ScrollbarPreference::Auto;
  }
  const nsAttrValue* attrValue = aOwner->GetParsedAttr(nsGkAtoms::scrolling);
  return nsGenericHTMLFrameElement::MapScrollingAttribute(attrValue);
}

static CSSIntSize GetMarginAttributes(const Element* aOwner) {
  CSSIntSize result(-1, -1);
  auto* content = nsGenericHTMLElement::FromNodeOrNull(aOwner);
  if (!content) {
    return result;
  }
  const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::marginwidth);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    result.width = attr->GetIntegerValue();
  }
  attr = content->GetParsedAttr(nsGkAtoms::marginheight);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    result.height = attr->GetIntegerValue();
  }
  return result;
}

bool nsFrameLoader::Show(nsSubDocumentFrame* frame) {
  if (mInShow) {
    return false;
  }
  mInShow = true;

  auto resetInShow = mozilla::MakeScopeExit([&] { mInShow = false; });

  ScreenIntSize size = frame->GetSubdocumentSize();
  if (IsRemoteFrame()) {
    // FIXME(bug 1588791): For fission iframes we need to pass down the
    // scrollbar preferences.
    return ShowRemoteFrame(size, frame);
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return false;
  }
  nsDocShell* ds = GetDocShell();
  MOZ_ASSERT(ds, "MaybeCreateDocShell succeeded, but null docShell");
  if (!ds) {
    return false;
  }

  ds->SetScrollbarPreference(GetScrollbarPreference(mOwnerContent));
  const bool marginsChanged =
      ds->UpdateFrameMargins(GetMarginAttributes(mOwnerContent));
  if (PresShell* presShell = ds->GetPresShell()) {
    // Ensure root scroll frame is reflowed in case margins have changed
    if (marginsChanged) {
      if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
        presShell->FrameNeedsReflow(rootScrollFrame, IntrinsicDirty::Resize,
                                    NS_FRAME_IS_DIRTY);
      }
    }
    return true;
  }

  nsView* view = frame->EnsureInnerView();
  if (!view) return false;

  RefPtr<nsDocShell> baseWindow = GetDocShell();
  baseWindow->InitWindow(nullptr, view->GetWidget(), 0, 0, size.width,
                         size.height);
  baseWindow->SetVisibility(true);
  NS_ENSURE_TRUE(GetDocShell(), false);

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  if (RefPtr<PresShell> presShell = GetDocShell()->GetPresShell()) {
    Document* doc = presShell->GetDocument();
    nsHTMLDocument* htmlDoc =
        doc && doc->IsHTMLOrXHTML() ? doc->AsHTMLDocument() : nullptr;

    if (htmlDoc) {
      nsAutoString designMode;
      htmlDoc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        // Hold on to the editor object to let the document reattach to the
        // same editor object, instead of creating a new one.
        RefPtr<HTMLEditor> htmlEditor = GetDocShell()->GetHTMLEditor();
        Unused << htmlEditor;
        htmlDoc->SetDesignMode(u"off"_ns, Nothing(), IgnoreErrors());

        htmlDoc->SetDesignMode(u"on"_ns, Nothing(), IgnoreErrors());
      } else {
        // Re-initialize the presentation for contenteditable documents
        bool editable = false, hasEditingSession = false;
        GetDocShell()->GetEditable(&editable);
        GetDocShell()->GetHasEditingSession(&hasEditingSession);
        RefPtr<HTMLEditor> htmlEditor = GetDocShell()->GetHTMLEditor();
        if (editable && hasEditingSession && htmlEditor) {
          htmlEditor->PostCreate();
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

void nsFrameLoader::MarginsChanged() {
  // We assume that the margins are always zero for remote frames.
  if (IsRemoteFrame()) {
    return;
  }

  nsDocShell* docShell = GetDocShell();
  // If there's no docshell, we're probably not up and running yet.
  // nsFrameLoader::Show() will take care of setting the right
  // margins.
  if (!docShell) {
    return;
  }

  if (!docShell->UpdateFrameMargins(GetMarginAttributes(mOwnerContent))) {
    return;
  }

  // There's a cached property declaration block
  // that needs to be updated
  if (Document* doc = docShell->GetDocument()) {
    for (nsINode* cur = doc; cur; cur = cur->GetNextNode()) {
      if (cur->IsHTMLElement(nsGkAtoms::body)) {
        static_cast<HTMLBodyElement*>(cur)->ClearMappedServoStyle();
      }
    }
  }

  // Trigger a restyle if there's a prescontext
  // FIXME: This could do something much less expensive.
  if (nsPresContext* presContext = docShell->GetPresContext()) {
    // rebuild, because now the same nsMappedAttributes* will produce
    // a different style
    presContext->RebuildAllStyleData(nsChangeHint(0),
                                     RestyleHint::RestyleSubtree());
  }
}

bool nsFrameLoader::ShowRemoteFrame(const ScreenIntSize& size,
                                    nsSubDocumentFrame* aFrame) {
  AUTO_PROFILER_LABEL("nsFrameLoader::ShowRemoteFrame", OTHER);
  NS_ASSERTION(IsRemoteFrame(),
               "ShowRemote only makes sense on remote frames.");

  if (!EnsureRemoteBrowser()) {
    NS_ERROR("Couldn't create child process.");
    return false;
  }

  // FIXME/bug 589337: Show()/Hide() is pretty expensive for
  // cross-process layers; need to figure out what behavior we really
  // want here.  For now, hack.
  if (!mRemoteBrowserShown) {
    if (!mOwnerContent || !mOwnerContent->GetComposedDoc()) {
      return false;
    }

    // We never want to host remote frameloaders in simple popups, like menus.
    nsIWidget* widget = nsContentUtils::WidgetForContent(mOwnerContent);
    if (!widget || static_cast<nsBaseWidget*>(widget)->IsSmallPopup()) {
      return false;
    }

    if (BrowserHost* bh = mRemoteBrowser->AsBrowserHost()) {
      RefPtr<BrowsingContext> bc = bh->GetBrowsingContext()->Top();

      // Set to the current activation of the window.
      bc->SetIsActiveBrowserWindow(bc->GetIsActiveBrowserWindow());
    }

    nsCOMPtr<nsISupports> container = mOwnerContent->OwnerDoc()->GetContainer();
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
    nsCOMPtr<nsIWidget> mainWidget;
    baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    nsSizeMode sizeMode =
        mainWidget ? mainWidget->SizeMode() : nsSizeMode_Normal;
    OwnerShowInfo info(size, GetScrollbarPreference(mOwnerContent), sizeMode);
    if (!mRemoteBrowser->Show(info)) {
      return false;
    }
    mRemoteBrowserShown = true;

    // This notification doesn't apply to fission, apparently.
    if (!GetBrowserBridgeChild()) {
      if (nsCOMPtr<nsIObserverService> os = services::GetObserverService()) {
        os->NotifyObservers(ToSupports(this), "remote-browser-shown", nullptr);
      }
      ProcessPriorityManager::RemoteBrowserFrameShown(this);
    }
  } else {
    nsIntRect dimensions;
    NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), false);

    // Don't show remote iframe if we are waiting for the completion of reflow.
    if (!aFrame || !(aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
  }

  return true;
}

void nsFrameLoader::Hide() {
  if (mHideCalled) {
    return;
  }
  if (mInShow) {
    mHideCalled = true;
    return;
  }

  if (!GetDocShell()) {
    return;
  }

  GetDocShell()->MaybeClearStorageAccessFlag();

  nsCOMPtr<nsIContentViewer> contentViewer;
  GetDocShell()->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) contentViewer->SetSticky(false);

  RefPtr<nsDocShell> baseWin = GetDocShell();
  baseWin->SetVisibility(false);
  baseWin->SetParentWidget(nullptr);
}

void nsFrameLoader::ForceLayoutIfNecessary() {
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (!frame) {
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  if (!presContext) {
    return;
  }

  // Only force the layout flush if the frameloader hasn't ever been
  // run through layout.
  if (frame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    if (RefPtr<PresShell> presShell = presContext->GetPresShell()) {
      presShell->FlushPendingNotifications(FlushType::Layout);
    }
  }
}

nsresult nsFrameLoader::SwapWithOtherRemoteLoader(
    nsFrameLoader* aOther, nsFrameLoaderOwner* aThisOwner,
    nsFrameLoaderOwner* aOtherOwner) {
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv = ourContent->NodePrincipal()->Equals(
      otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  Document* ourDoc = ourContent->GetComposedDoc();
  Document* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  PresShell* ourPresShell = ourDoc->GetPresShell();
  PresShell* otherPresShell = otherDoc->GetPresShell();
  if (!ourPresShell || !otherPresShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  auto* browserParent = GetBrowserParent();
  auto* otherBrowserParent = aOther->GetBrowserParent();

  if (!browserParent || !otherBrowserParent) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<BrowsingContext> ourBc = browserParent->GetBrowsingContext();
  RefPtr<BrowsingContext> otherBc = otherBrowserParent->GetBrowsingContext();

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning element, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes = ourBc->OriginAttributesRef();
  rv = PopulateOriginContextIdsFromAttributes(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes otherOriginAttributes = otherBc->OriginAttributesRef();
  rv = aOther->PopulateOriginContextIdsFromAttributes(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ourOriginAttributes.EqualsIgnoringFPD(otherOriginAttributes)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasHistory =
      mIsTopLevelContent && ourContent->IsXULElement(nsGkAtoms::browser) &&
      !ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  bool otherHasHistory =
      aOther->mIsTopLevelContent &&
      otherContent->IsXULElement(nsGkAtoms::browser) &&
      !otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  if (ourHasHistory != otherHasHistory) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mInSwap = aOther->mInSwap = true;

  // NOTE(emilio): This doesn't have to flush because the caller does already.
  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    mInSwap = aOther->mInSwap = false;
    return rv;
  }

  nsCOMPtr<nsIBrowserDOMWindow> otherBrowserDOMWindow =
      otherBrowserParent->GetBrowserDOMWindow();
  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWindow =
      browserParent->GetBrowserDOMWindow();

  if (!!otherBrowserDOMWindow != !!browserDOMWindow) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  otherBrowserParent->SetBrowserDOMWindow(browserDOMWindow);
  browserParent->SetBrowserDOMWindow(otherBrowserDOMWindow);

  MaybeUpdatePrimaryBrowserParent(eBrowserParentRemoved);
  aOther->MaybeUpdatePrimaryBrowserParent(eBrowserParentRemoved);

  if (mozilla::BFCacheInParent() && XRE_IsParentProcess()) {
    // nsFrameLoaders in session history can't be moved to another owner since
    // there are no corresponging message managers on which swap can be done.
    // See the line mMessageManager.swap(aOther->mMessageManager); below.
    auto evict = [](nsFrameLoader* aFrameLoader) {
      if (BrowsingContext* bc =
              aFrameLoader->GetMaybePendingBrowsingContext()) {
        nsCOMPtr<nsISHistory> shistory = bc->Canonical()->GetSessionHistory();
        if (shistory) {
          shistory->EvictAllContentViewers();
        }
      }
    };
    evict(this);
    evict(aOther);
  }

  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  browserParent->SetOwnerElement(otherContent);
  otherBrowserParent->SetOwnerElement(ourContent);

  // Update window activation state for the swapped owner content.
  bool ourActive = otherBc->GetIsActiveBrowserWindow();
  bool otherActive = ourBc->GetIsActiveBrowserWindow();
  if (ourBc->IsTop()) {
    ourBc->SetIsActiveBrowserWindow(otherActive);
  }
  if (otherBc->IsTop()) {
    otherBc->SetIsActiveBrowserWindow(ourActive);
  }

  MaybeUpdatePrimaryBrowserParent(eBrowserParentChanged);
  aOther->MaybeUpdatePrimaryBrowserParent(eBrowserParentChanged);

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap and setup things in parent message managers.
  if (ourMessageManager) {
    ourMessageManager->SetCallback(aOther);
  }
  if (otherMessageManager) {
    otherMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // XXXsmaug what should be done to JSWindowActorParent objects when swapping
  // frameloaders? Currently they leak very easily, bug 1697918.

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  RefPtr<nsFrameLoader> kungFuDeathGrip(this);
  aThisOwner->SetFrameLoader(aOther);
  aOtherOwner->SetFrameLoader(kungFuDeathGrip);

  ourFrameFrame->EndSwapDocShells(otherFrame);

  ourPresShell->BackingScaleFactorChanged();
  otherPresShell->BackingScaleFactorChanged();

  // Initialize browser API if needed now that owner content has changed.
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  mInSwap = aOther->mInSwap = false;

  // Send an updated tab context since owner content type may have changed.
  MutableTabContext ourContext;
  rv = GetNewTabContext(&ourContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MutableTabContext otherContext;
  rv = aOther->GetNewTabContext(&otherContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Unused << browserParent->SendSwappedWithOtherRemoteLoader(
      ourContext.AsIPCTabContext());
  Unused << otherBrowserParent->SendSwappedWithOtherRemoteLoader(
      otherContext.AsIPCTabContext());
  return NS_OK;
}

class MOZ_RAII AutoResetInFrameSwap final {
 public:
  AutoResetInFrameSwap(nsFrameLoader* aThisFrameLoader,
                       nsFrameLoader* aOtherFrameLoader,
                       nsDocShell* aThisDocShell, nsDocShell* aOtherDocShell,
                       EventTarget* aThisEventTarget,
                       EventTarget* aOtherEventTarget)
      : mThisFrameLoader(aThisFrameLoader),
        mOtherFrameLoader(aOtherFrameLoader),
        mThisDocShell(aThisDocShell),
        mOtherDocShell(aOtherDocShell),
        mThisEventTarget(aThisEventTarget),
        mOtherEventTarget(aOtherEventTarget) {
    mThisFrameLoader->mInSwap = true;
    mOtherFrameLoader->mInSwap = true;
    mThisDocShell->SetInFrameSwap(true);
    mOtherDocShell->SetInFrameSwap(true);

    // Fire pageshow events on still-loading pages, and then fire pagehide
    // events.  Note that we do NOT fire these in the normal way, but just fire
    // them on the chrome event handlers.
    nsContentUtils::FirePageShowEventForFrameLoaderSwap(
        mThisDocShell, mThisEventTarget, false);
    nsContentUtils::FirePageShowEventForFrameLoaderSwap(
        mOtherDocShell, mOtherEventTarget, false);
    nsContentUtils::FirePageHideEventForFrameLoaderSwap(mThisDocShell,
                                                        mThisEventTarget);
    nsContentUtils::FirePageHideEventForFrameLoaderSwap(mOtherDocShell,
                                                        mOtherEventTarget);
  }

  ~AutoResetInFrameSwap() {
    nsContentUtils::FirePageShowEventForFrameLoaderSwap(mThisDocShell,
                                                        mThisEventTarget, true);
    nsContentUtils::FirePageShowEventForFrameLoaderSwap(
        mOtherDocShell, mOtherEventTarget, true);

    mThisFrameLoader->mInSwap = false;
    mOtherFrameLoader->mInSwap = false;
    mThisDocShell->SetInFrameSwap(false);
    mOtherDocShell->SetInFrameSwap(false);

    // This is needed to get visibility state right in cases when we swapped a
    // visible tab (foreground in visible window) with a non-visible tab.
    if (RefPtr<Document> doc = mThisDocShell->GetDocument()) {
      doc->UpdateVisibilityState();
    }
    if (RefPtr<Document> doc = mOtherDocShell->GetDocument()) {
      doc->UpdateVisibilityState();
    }
  }

 private:
  RefPtr<nsFrameLoader> mThisFrameLoader;
  RefPtr<nsFrameLoader> mOtherFrameLoader;
  RefPtr<nsDocShell> mThisDocShell;
  RefPtr<nsDocShell> mOtherDocShell;
  nsCOMPtr<EventTarget> mThisEventTarget;
  nsCOMPtr<EventTarget> mOtherEventTarget;
};

nsresult nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                            nsFrameLoaderOwner* aThisOwner,
                                            nsFrameLoaderOwner* aOtherOwner) {
#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  NS_ENSURE_STATE(!mInShow && !aOther->mInShow);

  if (IsRemoteFrame() != aOther->IsRemoteFrame()) {
    NS_WARNING(
        "Swapping remote and non-remote frames is not currently supported");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<Element> ourContent = mOwnerContent;
  RefPtr<Element> otherContent = aOther->mOwnerContent;
  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsIFrame* ourFrame = ourContent->GetPrimaryFrame(FlushType::Frames);
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame(FlushType::Frames);
  if (!ourFrame || !otherFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Ensure the flushes above haven't changed all the world.
  if (ourContent != mOwnerContent || otherContent != aOther->mOwnerContent) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasSrcdoc = ourContent->IsHTMLElement(nsGkAtoms::iframe) &&
                      ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  bool otherHasSrcdoc =
      otherContent->IsHTMLElement(nsGkAtoms::iframe) &&
      otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (ourHasSrcdoc || otherHasSrcdoc) {
    // Ignore this case entirely for now, since we support XUL <-> HTML swapping
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourFullscreenAllowed = ourContent->IsXULElement() ||
                              (OwnerIsMozBrowserFrame() &&
                               ourContent->HasAttr(nsGkAtoms::allowfullscreen));
  bool otherFullscreenAllowed =
      otherContent->IsXULElement() ||
      (aOther->OwnerIsMozBrowserFrame() &&
       otherContent->HasAttr(nsGkAtoms::allowfullscreen));
  if (ourFullscreenAllowed != otherFullscreenAllowed) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsILoadContext* ourLoadContext = ourContent->OwnerDoc()->GetLoadContext();
  nsILoadContext* otherLoadContext = otherContent->OwnerDoc()->GetLoadContext();
  MOZ_ASSERT(ourLoadContext && otherLoadContext,
             "Swapping frames within dead documents?");
  if (ourLoadContext->UseRemoteTabs() != otherLoadContext->UseRemoteTabs()) {
    NS_WARNING("Can't swap between e10s and non-e10s windows");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (ourLoadContext->UseRemoteSubframes() !=
      otherLoadContext->UseRemoteSubframes()) {
    NS_WARNING("Can't swap between fission and non-fission windows");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Divert to a separate path for the remaining steps in the remote case
  if (IsRemoteFrame()) {
    MOZ_ASSERT(aOther->IsRemoteFrame());
    return SwapWithOtherRemoteLoader(aOther, aThisOwner, aOtherOwner);
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv = ourContent->NodePrincipal()->Equals(
      otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  RefPtr<nsDocShell> ourDocshell = GetExistingDocShell();
  RefPtr<nsDocShell> otherDocshell = aOther->GetExistingDocShell();
  if (!ourDocshell || !otherDocshell) {
    // How odd
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // To avoid having to mess with session history, avoid swapping
  // frameloaders that don't correspond to root same-type docshells,
  // unless both roots have session history disabled.
  nsCOMPtr<nsIDocShellTreeItem> ourRootTreeItem, otherRootTreeItem;
  ourDocshell->GetInProcessSameTypeRootTreeItem(
      getter_AddRefs(ourRootTreeItem));
  otherDocshell->GetInProcessSameTypeRootTreeItem(
      getter_AddRefs(otherRootTreeItem));
  nsCOMPtr<nsIWebNavigation> ourRootWebnav = do_QueryInterface(ourRootTreeItem);
  nsCOMPtr<nsIWebNavigation> otherRootWebnav =
      do_QueryInterface(otherRootTreeItem);

  if (!ourRootWebnav || !otherRootWebnav) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<ChildSHistory> ourHistory = ourRootWebnav->GetSessionHistory();
  RefPtr<ChildSHistory> otherHistory = otherRootWebnav->GetSessionHistory();

  if ((ourRootTreeItem != ourDocshell || otherRootTreeItem != otherDocshell) &&
      (ourHistory || otherHistory)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<BrowsingContext> ourBc = ourDocshell->GetBrowsingContext();
  RefPtr<BrowsingContext> otherBc = otherDocshell->GetBrowsingContext();

  // Also make sure that the two BrowsingContexts are the same type. Otherwise
  // swapping is certainly not safe. If this needs to be changed then
  // the code below needs to be audited as it assumes identical types.
  if (ourBc->GetType() != otherBc->GetType()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // We ensure that BCs are either both top frames or both subframes.
  if (ourBc->IsTop() != otherBc->IsTop()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // One more twist here.  Setting up the right treeowners in a heterogeneous
  // tree is a bit of a pain.  So make sure that if `ourBc->GetType()` is not
  // nsIDocShellTreeItem::typeContent then all of our descendants are the same
  // type as us.
  if (!ourBc->IsContent() &&
      (!AllDescendantsOfType(ourBc, ourBc->GetType()) ||
       !AllDescendantsOfType(otherBc, otherBc->GetType()))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Save off the tree owners, frame elements, chrome event handlers, and
  // docshell and document parents before doing anything else.
  nsCOMPtr<nsIDocShellTreeOwner> ourOwner, otherOwner;
  ourDocshell->GetTreeOwner(getter_AddRefs(ourOwner));
  otherDocshell->GetTreeOwner(getter_AddRefs(otherOwner));
  // Note: it's OK to have null treeowners.

  nsCOMPtr<nsIDocShellTreeItem> ourParentItem, otherParentItem;
  ourDocshell->GetInProcessParent(getter_AddRefs(ourParentItem));
  otherDocshell->GetInProcessParent(getter_AddRefs(otherParentItem));
  if (!ourParentItem || !otherParentItem) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocshell->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> otherWindow = otherDocshell->GetWindow();

  nsCOMPtr<Element> ourFrameElement = ourWindow->GetFrameElementInternal();
  nsCOMPtr<Element> otherFrameElement = otherWindow->GetFrameElementInternal();

  nsCOMPtr<EventTarget> ourChromeEventHandler =
      ourWindow->GetChromeEventHandler();
  nsCOMPtr<EventTarget> otherChromeEventHandler =
      otherWindow->GetChromeEventHandler();

  nsCOMPtr<EventTarget> ourEventTarget = ourWindow->GetParentTarget();
  nsCOMPtr<EventTarget> otherEventTarget = otherWindow->GetParentTarget();

  NS_ASSERTION(SameCOMIdentity(ourFrameElement, ourContent) &&
                   SameCOMIdentity(otherFrameElement, otherContent) &&
                   SameCOMIdentity(ourChromeEventHandler, ourContent) &&
                   SameCOMIdentity(otherChromeEventHandler, otherContent),
               "How did that happen, exactly?");

  nsCOMPtr<Document> ourChildDocument = ourWindow->GetExtantDoc();
  nsCOMPtr<Document> otherChildDocument = otherWindow->GetExtantDoc();
  if (!ourChildDocument || !otherChildDocument) {
    // This shouldn't be happening
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<Document> ourParentDocument =
      ourChildDocument->GetInProcessParentDocument();
  nsCOMPtr<Document> otherParentDocument =
      otherChildDocument->GetInProcessParentDocument();

  // Make sure to swap docshells between the two frames.
  Document* ourDoc = ourContent->GetComposedDoc();
  Document* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_ASSERTION(ourDoc == ourParentDocument, "Unexpected parent document");
  NS_ASSERTION(otherDoc == otherParentDocument, "Unexpected parent document");

  PresShell* ourPresShell = ourDoc->GetPresShell();
  PresShell* otherPresShell = otherDoc->GetPresShell();
  if (!ourPresShell || !otherPresShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning element, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes = ourDocshell->GetOriginAttributes();
  rv = PopulateOriginContextIdsFromAttributes(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes otherOriginAttributes = otherDocshell->GetOriginAttributes();
  rv = aOther->PopulateOriginContextIdsFromAttributes(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  if (ourOriginAttributes != otherOriginAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  AutoResetInFrameSwap autoFrameSwap(this, aOther, ourDocshell, otherDocshell,
                                     ourEventTarget, otherEventTarget);

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // OK.  First begin to swap the docshells in the two nsIFrames
  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  // Now move the docshells to the right docshell trees.  Note that this
  // resets their treeowners to null.
  ourParentItem->RemoveChild(ourDocshell);
  otherParentItem->RemoveChild(otherDocshell);
  if (ourBc->IsContent()) {
    ourOwner->ContentShellRemoved(ourDocshell);
    otherOwner->ContentShellRemoved(otherDocshell);
  }

  ourParentItem->AddChild(otherDocshell);
  otherParentItem->AddChild(ourDocshell);

  // Restore the correct chrome event handlers.
  ourDocshell->SetChromeEventHandler(otherChromeEventHandler);
  otherDocshell->SetChromeEventHandler(ourChromeEventHandler);
  // Restore the correct treeowners
  // (and also chrome event handlers for content frames only).
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
      ourDocshell, otherOwner,
      ourBc->IsContent() ? otherChromeEventHandler.get() : nullptr);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(
      otherDocshell, ourOwner,
      ourBc->IsContent() ? ourChromeEventHandler.get() : nullptr);

  // Switch the owner content before we start calling AddTreeItemToTreeOwner.
  // Note that we rely on this to deal with setting mObservingOwnerContent to
  // false and calling RemoveMutationObserver as needed.
  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  AddTreeItemToTreeOwner(ourDocshell, otherOwner);
  aOther->AddTreeItemToTreeOwner(otherDocshell, ourOwner);

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

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap pointers in child message managers.
  if (mChildMessageManager) {
    InProcessBrowserChildMessageManager* browserChild = mChildMessageManager;
    browserChild->SetOwner(otherContent);
    browserChild->SetChromeMessageManager(otherMessageManager);
  }
  if (aOther->mChildMessageManager) {
    InProcessBrowserChildMessageManager* otherBrowserChild =
        aOther->mChildMessageManager;
    otherBrowserChild->SetOwner(ourContent);
    otherBrowserChild->SetChromeMessageManager(ourMessageManager);
  }
  // Swap and setup things in parent message managers.
  if (mMessageManager) {
    mMessageManager->SetCallback(aOther);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  RefPtr<nsFrameLoader> kungFuDeathGrip(this);
  aThisOwner->SetFrameLoader(aOther);
  aOtherOwner->SetFrameLoader(kungFuDeathGrip);

  // Drop any cached content viewers in the two session histories.
  if (ourHistory) {
    ourHistory->EvictLocalContentViewers();
  }
  if (otherHistory) {
    otherHistory->EvictLocalContentViewers();
  }

  NS_ASSERTION(ourFrame == ourContent->GetPrimaryFrame() &&
                   otherFrame == otherContent->GetPrimaryFrame(),
               "changed primary frame");

  ourFrameFrame->EndSwapDocShells(otherFrame);

  // If the content being swapped came from windows on two screens with
  // incompatible backing resolution (e.g. dragging a tab between windows on
  // hi-dpi and low-dpi screens), it will have style data that is based on
  // the wrong appUnitsPerDevPixel value. So we tell the PresShells that their
  // backing scale factor may have changed. (Bug 822266)
  ourFrame->PresShell()->BackingScaleFactorChanged();
  otherFrame->PresShell()->BackingScaleFactorChanged();

  // Initialize browser API if needed now that owner content has changed
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  return NS_OK;
}

void nsFrameLoader::Destroy(bool aForProcessSwitch) {
  StartDestroy(aForProcessSwitch);
}

class nsFrameLoaderDestroyRunnable : public Runnable {
  enum DestroyPhase {
    // See the implementation of Run for an explanation of these phases.
    eDestroyDocShell,
    eWaitForUnloadMessage,
    eDestroyComplete
  };

  RefPtr<nsFrameLoader> mFrameLoader;
  DestroyPhase mPhase;

 public:
  explicit nsFrameLoaderDestroyRunnable(nsFrameLoader* aFrameLoader)
      : mozilla::Runnable("nsFrameLoaderDestroyRunnable"),
        mFrameLoader(aFrameLoader),
        mPhase(eDestroyDocShell) {}

  NS_IMETHOD Run() override;
};

void nsFrameLoader::StartDestroy(bool aForProcessSwitch) {
  // nsFrameLoader::StartDestroy is called just before the frameloader is
  // detached from the <browser> element. Destruction continues in phases via
  // the nsFrameLoaderDestroyRunnable.

  if (mDestroyCalled) {
    return;
  }
  mDestroyCalled = true;

  // Request a full tab state flush if the tab is closing.
  //
  // XXX If we find that we need to do Session Store cleanup for the frameloader
  // that's going away, we should unconditionally do the flush here, but include
  // the |aForProcessSwitch| flag in the completion notification.
  if (!aForProcessSwitch) {
    RequestFinalTabStateFlush();
  }

  // After this point, we return an error when trying to send a message using
  // the message manager on the frame.
  if (mMessageManager) {
    mMessageManager->Close();
  }

  // Retain references to the <browser> element and the frameloader in case we
  // receive any messages from the message manager on the frame. These
  // references are dropped in DestroyComplete.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = mOwnerContent;
    if (auto* browserParent = GetBrowserParent()) {
      browserParent->CacheFrameLoader(this);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(this);
    }
  }

  // If the BrowserParent has installed any event listeners on the window, this
  // is its last chance to remove them while we're still in the document.
  if (auto* browserParent = GetBrowserParent()) {
    browserParent->RemoveWindowListeners();
  }

  nsCOMPtr<Document> doc;
  bool dynamicSubframeRemoval = false;
  if (mOwnerContent) {
    doc = mOwnerContent->OwnerDoc();
    dynamicSubframeRemoval = !aForProcessSwitch &&
                             mPendingBrowsingContext->IsSubframe() &&
                             !doc->InUnlinkOrDeletion();
    doc->SetSubDocumentFor(mOwnerContent, nullptr);
    MaybeUpdatePrimaryBrowserParent(eBrowserParentRemoved);

    nsCOMPtr<nsFrameLoaderOwner> owner = do_QueryInterface(mOwnerContent);
    owner->FrameLoaderDestroying(this);
    SetOwnerContent(nullptr);
  }

  // Seems like this is a dynamic frame removal.
  if (dynamicSubframeRemoval) {
    BrowsingContext* browsingContext = GetExtantBrowsingContext();
    if (browsingContext) {
      RefPtr<ChildSHistory> childSHistory =
          browsingContext->Top()->GetChildSessionHistory();
      if (childSHistory) {
        if (mozilla::SessionHistoryInParent()) {
          uint32_t addedEntries = 0;
          browsingContext->PreOrderWalk([&addedEntries](BrowsingContext* aBC) {
            // The initial load doesn't increase history length.
            addedEntries += aBC->GetHistoryEntryCount() - 1;
          });

          nsID changeID = {};
          if (addedEntries > 0) {
            ChildSHistory* shistory =
                browsingContext->Top()->GetChildSessionHistory();
            if (shistory) {
              changeID = shistory->AddPendingHistoryChange(0, -addedEntries);
            }
          }
          browsingContext->RemoveFromSessionHistory(changeID);
        } else {
          AutoTArray<nsID, 16> ids({browsingContext->GetHistoryID()});
          childSHistory->LegacySHistory()->RemoveEntries(
              ids, childSHistory->Index());
        }
      }
    }
  }

  // Let the tree owner know we're gone.
  if (mIsTopLevelContent) {
    if (GetDocShell()) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      GetDocShell()->GetInProcessParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsIDocShellTreeOwner> owner = do_GetInterface(parentItem);
      if (owner) {
        owner->ContentShellRemoved(GetDocShell());
      }
    }
  }

  // Let our window know that we are gone
  if (GetDocShell()) {
    nsCOMPtr<nsPIDOMWindowOuter> win_private(GetDocShell()->GetWindow());
    if (win_private) {
      win_private->SetFrameElementInternal(nullptr);
    }
  }

  nsCOMPtr<nsIRunnable> destroyRunnable =
      new nsFrameLoaderDestroyRunnable(this);
  if (mNeedsAsyncDestroy || !doc ||
      NS_FAILED(doc->FinalizeFrameLoader(this, destroyRunnable))) {
    NS_DispatchToCurrentThread(destroyRunnable);
  }
}

nsresult nsFrameLoaderDestroyRunnable::Run() {
  switch (mPhase) {
    case eDestroyDocShell:
      mFrameLoader->DestroyDocShell();

      // In the out-of-process case, BrowserParent will eventually call
      // DestroyComplete once it receives a __delete__ message from the child.
      // In the in-process case, or if the BrowserParent was already destroyed,
      // we dispatch a series of runnables to ensure that DestroyComplete gets
      // called at the right time. The frame loader is kept alive by
      // mFrameLoader during this time.
      if (!mFrameLoader->GetRemoteBrowser() ||
          !mFrameLoader->GetRemoteBrowser()->CanRecv()) {
        // When the docshell is destroyed, NotifyWindowIDDestroyed is called to
        // asynchronously notify {outer,inner}-window-destroyed via a runnable.
        // We don't want DestroyComplete to run until after those runnables have
        // run. Since we're enqueueing ourselves after the window-destroyed
        // runnables are enqueued, we're guaranteed to run after.
        mPhase = eWaitForUnloadMessage;
        NS_DispatchToCurrentThread(this);
      }
      break;

    case eWaitForUnloadMessage:
      // The *-window-destroyed observers have finished running at this
      // point. However, it's possible that a *-window-destroyed observer might
      // have sent a message using the message manager. These messages might not
      // have been processed yet. So we enqueue ourselves again to ensure that
      // DestroyComplete runs after all messages sent by *-window-destroyed
      // observers have been processed.
      mPhase = eDestroyComplete;
      NS_DispatchToCurrentThread(this);
      break;

    case eDestroyComplete:
      // Now that all messages sent by unload listeners and window destroyed
      // observers have been processed, we disconnect the message manager and
      // finish destruction.
      mFrameLoader->DestroyComplete();
      break;
  }

  return NS_OK;
}

void nsFrameLoader::DestroyDocShell() {
  // This code runs after the frameloader has been detached from the <browser>
  // element. We postpone this work because we may not be allowed to run
  // script at that time.

  // Ask the BrowserChild to fire the frame script "unload" event, destroy its
  // docshell, and finally destroy the PBrowser actor. This eventually leads to
  // nsFrameLoader::DestroyComplete being called.
  if (mRemoteBrowser) {
    mRemoteBrowser->DestroyStart();
  }

  // Fire the "unload" event if we're in-process.
  if (mChildMessageManager) {
    mChildMessageManager->FireUnloadEvent();
  }

  if (mSessionStoreListener) {
    mSessionStoreListener->RemoveListeners();
    mSessionStoreListener = nullptr;
  }

  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->Stop();
    mSessionStoreChangeListener = nullptr;
  }

  // Destroy the docshell.
  if (GetDocShell()) {
    GetDocShell()->Destroy();
  }

  if (!mWillChangeProcess && mPendingBrowsingContext &&
      mPendingBrowsingContext->EverAttached()) {
    mPendingBrowsingContext->Detach();
  }

  mPendingBrowsingContext = nullptr;
  mDocShell = nullptr;

  if (mChildMessageManager) {
    // Stop handling events in the in-process frame script.
    mChildMessageManager->DisconnectEventListeners();
  }
}

void nsFrameLoader::DestroyComplete() {
  // We get here, as part of StartDestroy, after the docshell has been destroyed
  // and all message manager messages sent during docshell destruction have been
  // dispatched.  We also get here if the child process crashes. In the latter
  // case, StartDestroy might not have been called.

  // Drop the strong references created in StartDestroy.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = nullptr;
    if (auto* browserParent = GetBrowserParent()) {
      browserParent->CacheFrameLoader(nullptr);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(nullptr);
    }
  }

  // Call BrowserParent::Destroy if we haven't already (in case of a crash).
  if (mRemoteBrowser) {
    mRemoteBrowser->DestroyComplete();
    mRemoteBrowser = nullptr;
  }

  if (mMessageManager) {
    mMessageManager->Disconnect();
  }

  if (mChildMessageManager) {
    mChildMessageManager->Disconnect();
  }

  mMessageManager = nullptr;
  mChildMessageManager = nullptr;
}

void nsFrameLoader::SetOwnerContent(Element* aContent) {
  if (mObservingOwnerContent) {
    mObservingOwnerContent = false;
    mOwnerContent->RemoveMutationObserver(this);
  }

  // XXXBFCache Need to update also all the non-current FrameLoaders in the
  // owner when moving a FrameLoader.
  // This temporary setup doesn't crash, but behaves badly with bfcached docs.
  if (RefPtr<nsFrameLoaderOwner> owner = do_QueryObject(mOwnerContent)) {
    owner->DetachFrameLoader(this);
  }

  mOwnerContent = aContent;

  if (RefPtr<nsFrameLoaderOwner> owner = do_QueryObject(mOwnerContent)) {
    owner->AttachFrameLoader(this);

#ifdef NIGHTLY_BUILD
    if (mozilla::BFCacheInParent() && XRE_IsParentProcess()) {
      if (BrowsingContext* bc = GetMaybePendingBrowsingContext()) {
        nsISHistory* shistory = bc->Canonical()->GetSessionHistory();
        if (shistory) {
          uint32_t count = shistory->GetCount();
          for (uint32_t i = 0; i < count; ++i) {
            nsCOMPtr<nsISHEntry> entry;
            shistory->GetEntryAtIndex(i, getter_AddRefs(entry));
            nsCOMPtr<SessionHistoryEntry> she = do_QueryInterface(entry);
            MOZ_RELEASE_ASSERT(!she || !she->GetFrameLoader());
          }
        }
      }
    }
#endif
  }

  if (mSessionStoreListener && mOwnerContent) {
    // mOwnerContent will only be null when the frame loader is being destroyed,
    // so the session store listener will be destroyed along with it.
    // XXX(farre): This probably needs to update the cache. See bug 1698497.
    mSessionStoreListener->SetOwnerContent(mOwnerContent);
  }

  if (RefPtr<BrowsingContext> browsingContext = GetExtantBrowsingContext()) {
    browsingContext->SetEmbedderElement(mOwnerContent);
  }

  if (mSessionStoreChangeListener) {
    // UpdateEventTargets will requery its browser contexts for event
    // targets, so this call needs to happen after the call to
    // SetEmbedderElement above.
    mSessionStoreChangeListener->UpdateEventTargets();
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  JS::RootedObject wrapper(jsapi.cx(), GetWrapper());
  if (wrapper) {
    JSAutoRealm ar(jsapi.cx(), wrapper);
    IgnoredErrorResult rv;
    UpdateReflectorGlobal(jsapi.cx(), wrapper, rv);
    Unused << NS_WARN_IF(rv.Failed());
  }
}

bool nsFrameLoader::OwnerIsMozBrowserFrame() {
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  return browserFrame ? browserFrame->GetReallyIsBrowser() : false;
}

nsIContent* nsFrameLoader::GetParentObject() const { return mOwnerContent; }

void nsFrameLoader::AssertSafeToInit() {
  MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::IsSafeToRunScript() ||
                            mOwnerContent->OwnerDoc()->IsStaticDocument(),
                        "FrameLoader should never be initialized during "
                        "document update or reflow!");
}

nsresult nsFrameLoader::MaybeCreateDocShell() {
  if (GetDocShell()) {
    return NS_OK;
  }
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  NS_ENSURE_STATE(!mDestroyCalled);

  AssertSafeToInit();

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  Document* doc = mOwnerContent->OwnerDoc();

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  // If we've already tried to initialize and failed, don't try again.
  if (mInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mInitialized = true;

  // Check if the document still has a window since it is possible for an
  // iframe to be inserted and cause the creation of the docshell in a
  // partially unloaded document (see Bug 1305237 comment 127).
  if (!doc->IsStaticDocument() &&
      (!doc->GetWindow() || !mOwnerContent->IsInComposedDoc())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<nsDocShell> parentDocShell = nsDocShell::Cast(doc->GetDocShell());
  if (NS_WARN_IF(!parentDocShell)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (doc->GetWindowContext()->IsDiscarded() ||
      parentDocShell->GetBrowsingContext()->IsDiscarded()) {
    // Don't allow subframe loads in discarded contexts.
    // (see bug 1652085, bug 1656854)
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!EnsureBrowsingContextAttached()) {
    return NS_ERROR_FAILURE;
  }

  mPendingBrowsingContext->SetEmbedderElement(mOwnerContent);

  // nsDocShell::Create will attach itself to the passed browsing
  // context inside of nsDocShell::Create
  RefPtr<nsDocShell> docShell = nsDocShell::Create(mPendingBrowsingContext);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  mDocShell = docShell;

  mPendingBrowsingContext->Embed();

  InvokeBrowsingContextReadyCallback();

  mIsTopLevelContent = mPendingBrowsingContext->IsTopContent();

  if (mIsTopLevelContent) {
    // Manually add ourselves to our parent's docshell, as BrowsingContext won't
    // have done this for us.
    //
    // XXX(nika): Consider removing the DocShellTree in the future, for
    // consistency between local and remote frames..
    parentDocShell->AddChild(docShell);
  }

  // Now that we are part of the DocShellTree, attach our DocShell to our
  // parent's TreeOwner.
  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentDocShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  AddTreeItemToTreeOwner(docShell, parentTreeOwner);

  // Make sure all nsDocShells have links back to the content element in the
  // nearest enclosing chrome shell.
  RefPtr<EventTarget> chromeEventHandler;
  bool parentIsContent = parentDocShell->GetBrowsingContext()->IsContent();
  if (parentIsContent) {
    // Our parent shell is a content shell. Get the chrome event handler from it
    // and use that for our shell as well.
    parentDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
  } else {
    // Our parent shell is a chrome shell. It is therefore our nearest enclosing
    // chrome shell.
    chromeEventHandler = mOwnerContent;
  }

  docShell->SetChromeEventHandler(chromeEventHandler);

  // This is nasty, this code (the docShell->GetWindow() below)
  // *must* come *after* the above call to
  // docShell->SetChromeEventHandler() for the global window to get
  // the right chrome event handler.

  // Tell the window about the frame that hosts it.
  nsCOMPtr<nsPIDOMWindowOuter> newWindow = docShell->GetWindow();
  if (NS_WARN_IF(!newWindow)) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  newWindow->SetFrameElementInternal(mOwnerContent);

  // Allow scripts to close the docshell if specified.
  if (mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
      mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                 nsGkAtoms::allowscriptstoclose,
                                 nsGkAtoms::_true, eCaseMatters)) {
    nsGlobalWindowOuter::Cast(newWindow)->AllowScriptsToClose();
  }

  if (!docShell->Initialize()) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_STATE(mOwnerContent);

  // If we are an in-process browser, we want to set up our session history.
  if (mIsTopLevelContent && mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory)) {
    // XXX(nika): Set this up more explicitly?
    mPendingBrowsingContext->InitSessionHistory();
  }

  // Apply sandbox flags even if our owner is not an iframe, as this copies
  // flags from our owning content's owning document.
  // Note: ApplySandboxFlags should be called after docShell->SetIsFrame
  // because we need to get the correct presentation URL in ApplySandboxFlags.
  uint32_t sandboxFlags = 0;
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromNode(mOwnerContent);
  if (iframe) {
    sandboxFlags = iframe->GetSandboxFlags();
  }
  ApplySandboxFlags(sandboxFlags);
  MOZ_ALWAYS_SUCCEEDS(mPendingBrowsingContext->SetInitialSandboxFlags(
      mPendingBrowsingContext->GetSandboxFlags()));

  if (OwnerIsMozBrowserFrame()) {
    // For inproc frames, set the docshell properties.
    nsAutoString name;
    if (mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
      docShell->SetName(name);
    }
  }

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

  // Previously we would forcibly create the initial about:blank document for
  // in-process content frames from a frame script which eagerly loaded in
  // every tab.  This lead to other frontend components growing dependencies on
  // the initial about:blank document being created eagerly.  See bug 1471327
  // for details.
  //
  // We also eagerly create the initial about:blank document for remote loads
  // separately when initializing BrowserChild.
  if (mIsTopLevelContent &&
      mPendingBrowsingContext->GetMessageManagerGroup() == u"browsers"_ns) {
    Unused << mDocShell->GetDocument();
  }

  return NS_OK;
}

void nsFrameLoader::GetURL(nsString& aURI, nsIPrincipal** aTriggeringPrincipal,
                           nsIContentSecurityPolicy** aCsp) {
  aURI.Truncate();
  // Within this function we default to using the NodePrincipal as the
  // triggeringPrincipal and the CSP of the document.
  // Expanded Principals however override the CSP of the document, hence
  // if frame->GetSrcTriggeringPrincipal() returns a valid principal, we
  // have to query the CSP from that Principal.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal = mOwnerContent->NodePrincipal();
  nsCOMPtr<nsIContentSecurityPolicy> csp = mOwnerContent->GetCsp();

  if (mOwnerContent->IsHTMLElement(nsGkAtoms::object)) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aURI);
    if (RefPtr<nsGenericHTMLFrameElement> frame =
            do_QueryObject(mOwnerContent)) {
      nsCOMPtr<nsIPrincipal> srcPrincipal = frame->GetSrcTriggeringPrincipal();
      if (srcPrincipal) {
        triggeringPrincipal = srcPrincipal;
        nsCOMPtr<nsIExpandedPrincipal> ep =
            do_QueryInterface(triggeringPrincipal);
        if (ep) {
          csp = ep->GetCsp();
        }
      }
    }
  }
  triggeringPrincipal.forget(aTriggeringPrincipal);
  csp.forget(aCsp);
}

nsresult nsFrameLoader::CheckForRecursiveLoad(nsIURI* aURI) {
  MOZ_ASSERT(!IsRemoteFrame(),
             "Shouldn't call CheckForRecursiveLoad on remote frames.");

  mDepthTooGreat = false;
  RefPtr<BrowsingContext> parentBC(
      mOwnerContent->OwnerDoc()->GetBrowsingContext());
  MOZ_ASSERT(parentBC, "How can we not have a parent here?");

  if (!parentBC->IsContent()) {
    return NS_OK;
  }

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  int32_t depth = 0;
  for (BrowsingContext* bc = parentBC; bc; bc = bc->GetParent()) {
    ++depth;
    if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
      mDepthTooGreat = true;
      NS_WARNING("Too many nested content frames so giving up");

      return NS_ERROR_UNEXPECTED;  // Too deep, give up!  (silently?)
    }
  }

  return NS_OK;
}

nsresult nsFrameLoader::GetWindowDimensions(nsIntRect& aRect) {
  if (!mOwnerContent) {
    return NS_ERROR_FAILURE;
  }

  // Need to get outer window position here
  Document* doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(win->GetDocShell());
  if (!parentAsItem) {
    return NS_ERROR_FAILURE;
  }

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

nsresult nsFrameLoader::UpdatePositionAndSize(nsSubDocumentFrame* aIFrame) {
  if (IsRemoteFrame()) {
    if (mRemoteBrowser) {
      ScreenIntSize size = aIFrame->GetSubdocumentSize();
      // If we were not able to show remote frame before, we should probably
      // retry now to send correct showInfo.
      if (!mRemoteBrowserShown) {
        ShowRemoteFrame(size, aIFrame);
      }
      nsIntRect dimensions;
      NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), NS_ERROR_FAILURE);
      mLazySize = size;
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
    return NS_OK;
  }
  UpdateBaseWindowPositionAndSize(aIFrame);
  return NS_OK;
}

void nsFrameLoader::SendIsUnderHiddenEmbedderElement(
    bool aIsUnderHiddenEmbedderElement) {
  MOZ_ASSERT(IsRemoteFrame());

  if (auto* browserBridgeChild = GetBrowserBridgeChild()) {
    browserBridgeChild->SetIsUnderHiddenEmbedderElement(
        aIsUnderHiddenEmbedderElement);
  }
}

void nsFrameLoader::PropagateIsUnderHiddenEmbedderElement(
    bool aIsUnderHiddenEmbedderElement) {
  bool isUnderHiddenEmbedderElement = true;
  if (Document* ownerDoc = GetOwnerDoc()) {
    if (PresShell* presShell = ownerDoc->GetPresShell()) {
      isUnderHiddenEmbedderElement = presShell->IsUnderHiddenEmbedderElement();
    }
  }

  isUnderHiddenEmbedderElement |= aIsUnderHiddenEmbedderElement;
  if (nsDocShell* docShell = GetExistingDocShell()) {
    if (PresShell* presShell = docShell->GetPresShell()) {
      presShell->SetIsUnderHiddenEmbedderElement(isUnderHiddenEmbedderElement);
    }
  } else {
    SendIsUnderHiddenEmbedderElement(isUnderHiddenEmbedderElement);
  }
}

void nsFrameLoader::UpdateBaseWindowPositionAndSize(
    nsSubDocumentFrame* aIFrame) {
  nsCOMPtr<nsIBaseWindow> baseWindow = GetDocShell(IgnoreErrors());

  // resize the sub document
  if (baseWindow) {
    int32_t x = 0;
    int32_t y = 0;

    AutoWeakFrame weakFrame(aIFrame);

    baseWindow->GetPosition(&x, &y);

    if (!weakFrame.IsAlive()) {
      // GetPosition() killed us
      return;
    }

    ScreenIntSize size = aIFrame->GetSubdocumentSize();
    mLazySize = size;

    baseWindow->SetPositionAndSize(x, y, size.width, size.height,
                                   nsIBaseWindow::eDelayResize);
  }
}

uint32_t nsFrameLoader::LazyWidth() const {
  uint32_t lazyWidth = mLazySize.width;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    lazyWidth = frame->PresContext()->DevPixelsToIntCSSPixels(lazyWidth);
  }

  return lazyWidth;
}

uint32_t nsFrameLoader::LazyHeight() const {
  uint32_t lazyHeight = mLazySize.height;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    lazyHeight = frame->PresContext()->DevPixelsToIntCSSPixels(lazyHeight);
  }

  return lazyHeight;
}

bool nsFrameLoader::EnsureRemoteBrowser() {
  MOZ_ASSERT(IsRemoteFrame());
  return mRemoteBrowser || TryRemoteBrowser();
}

bool nsFrameLoader::TryRemoteBrowserInternal() {
  NS_ASSERTION(!mRemoteBrowser,
               "TryRemoteBrowser called with a remote browser already?");
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                        "Remote subframes should only be created using the "
                        "`CanonicalBrowsingContext::ChangeRemoteness` API");

  AssertSafeToInit();

  if (!mOwnerContent) {
    return false;
  }

  // XXXsmaug Per spec (2014/08/21) frameloader should not work in case the
  //         element isn't in document, only in shadow dom, but that will change
  //         https://www.w3.org/Bugs/Public/show_bug.cgi?id=26365#c0
  RefPtr<Document> doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return false;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  // Graphics initialization code relies on having a frame for the
  // remote browser case, as we can be inside a popup, which is a different
  // widget.
  //
  // FIXME: Ideally this should be unconditional, but we skip if for <iframe
  // mozbrowser> because the old RDM ui depends on current behavior, and the
  // mozbrowser frame code is scheduled for deletion, see bug 1574886.
  if (!OwnerIsMozBrowserFrame() && !mOwnerContent->GetPrimaryFrame()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  // The flush could have initialized us.
  if (mRemoteBrowser) {
    return true;
  }

  // If we've already tried to initialize and failed, don't try again.
  if (mInitialized) {
    return false;
  }
  mInitialized = true;

  // Ensure the world hasn't changed that much as a result of that.
  if (!mOwnerContent || mOwnerContent->OwnerDoc() != doc ||
      !mOwnerContent->IsInComposedDoc()) {
    return false;
  }

  if (RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(mOwnerContent)) {
    RefPtr<nsFrameLoader> fl = flo->GetFrameLoader();
    if (fl != this) {
      MOZ_ASSERT_UNREACHABLE(
          "Got TryRemoteBrowserInternal but mOwnerContent already has a "
          "different frameloader?");
      return false;
    }
  }

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = doc->GetWindow();
  if (!parentWin) {
    return false;
  }

  nsCOMPtr<nsIDocShell> parentDocShell = parentWin->GetDocShell();
  if (!parentDocShell) {
    return false;
  }

  if (!EnsureBrowsingContextAttached()) {
    return false;
  }

  if (mPendingBrowsingContext->IsTop()) {
    mPendingBrowsingContext->InitSessionHistory();
  }

  // <iframe mozbrowser> gets to skip these checks.
  // iframes for JS plugins also get to skip these checks. We control the URL
  // that gets loaded, but the load is triggered from the document containing
  // the plugin.
  // out of process iframes also get to skip this check.
  if (!OwnerIsMozBrowserFrame() && !XRE_IsContentProcess()) {
    if (parentDocShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // Allow three exceptions to this rule :
      // - about:addons so it can load remote extension options pages
      // - about:preferences (in Thunderbird only) so it can load remote
      //     extension options pages for FileLink providers
      // - DevTools webext panels if DevTools is loaded in a content frame
      //
      // Note that the new frame's message manager will not be a child of the
      // chrome window message manager, and, the values of window.top and
      // window.parent will be different than they would be for a non-remote
      // frame.
      nsIURI* parentURI = parentWin->GetDocumentURI();
      if (!parentURI) {
        return false;
      }

      nsAutoCString specIgnoringRef;
      if (NS_FAILED(parentURI->GetSpecIgnoringRef(specIgnoringRef))) {
        return false;
      }

      if (!(specIgnoringRef.EqualsLiteral("about:addons") ||
            specIgnoringRef.EqualsLiteral(
                "chrome://mozapps/content/extensions/aboutaddons.html") ||
#ifdef MOZ_THUNDERBIRD
            specIgnoringRef.EqualsLiteral("about:preferences") ||
#endif
            specIgnoringRef.EqualsLiteral(
                "chrome://browser/content/webext-panels.xhtml"))) {
        return false;
      }
    }

    if (!mOwnerContent->IsXULElement()) {
      return false;
    }

    if (!mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                    nsGkAtoms::content, eIgnoreCase)) {
      return false;
    }
  }

  uint32_t chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentDocShell->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return false;
  }
  nsCOMPtr<nsIAppWindow> window(do_GetInterface(parentOwner));
  if (window && NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  AUTO_PROFILER_LABEL("nsFrameLoader::TryRemoteBrowser:Create", OTHER);

  MutableTabContext context;
  nsresult rv = GetNewTabContext(&context);
  NS_ENSURE_SUCCESS(rv, false);

  RefPtr<Element> ownerElement = mOwnerContent;

  RefPtr<BrowserParent> nextRemoteBrowser =
      mOpenWindowInfo ? mOpenWindowInfo->GetNextRemoteBrowser() : nullptr;
  if (nextRemoteBrowser) {
    mRemoteBrowser = new BrowserHost(nextRemoteBrowser);
    if (nextRemoteBrowser->GetOwnerElement()) {
      MOZ_ASSERT_UNREACHABLE("Shouldn't have an owner element before");
      return false;
    }
    nextRemoteBrowser->SetOwnerElement(ownerElement);
  } else {
    RefPtr<ContentParent> contentParent;
    if (mChildID != 0) {
      ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
      contentParent = cpm->GetContentProcessById(ContentParentId(mChildID));
    }
    mRemoteBrowser =
        ContentParent::CreateBrowser(context, ownerElement, mRemoteType,
                                     mPendingBrowsingContext, contentParent);
  }
  if (!mRemoteBrowser) {
    return false;
  }

  MOZ_DIAGNOSTIC_ASSERT(mPendingBrowsingContext ==
                        mRemoteBrowser->GetBrowsingContext());

  mRemoteBrowser->GetBrowsingContext()->Embed();
  InvokeBrowsingContextReadyCallback();

  // Grab the reference to the actor
  RefPtr<BrowserParent> browserParent = GetBrowserParent();

  MOZ_ASSERT(browserParent->CanSend(), "BrowserParent cannot send?");

  // We no longer need the remoteType attribute on the frame element.
  // The remoteType can be queried by asking the message manager instead.
  ownerElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, false);

  // Now that browserParent is set, we can initialize graphics
  browserParent->InitRendering();

  MaybeUpdatePrimaryBrowserParent(eBrowserParentChanged);

  mChildID = browserParent->Manager()->ChildID();

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  parentDocShell->GetInProcessRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
  nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);

  if (rootChromeWin) {
    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    browserParent->SetBrowserDOMWindow(browserDOMWin);
  }

  // For xul:browsers, update some settings based on attributes:
  if (mOwnerContent->IsXULElement()) {
    // Send down the name of the browser through browserParent if it is set.
    nsAutoString frameName;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    if (nsContentUtils::IsOverridingWindowName(frameName)) {
      MOZ_ALWAYS_SUCCEEDS(mPendingBrowsingContext->SetName(frameName));
    }
    // Allow scripts to close the window if the browser specified so:
    if (mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::allowscriptstoclose,
                                   nsGkAtoms::_true, eCaseMatters)) {
      Unused << browserParent->SendAllowScriptsToClose();
    }
  }

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

  return true;
}

bool nsFrameLoader::TryRemoteBrowser() {
  // Try to create the internal remote browser.
  if (TryRemoteBrowserInternal()) {
    return true;
  }

  // Check if we should report a browser-crashed error because the browser
  // failed to start.
  if (XRE_IsParentProcess() && mOwnerContent && mOwnerContent->IsXULElement()) {
    MaybeNotifyCrashed(nullptr, ContentParentId(), nullptr);
  }

  return false;
}

nsIFrame* nsFrameLoader::GetPrimaryFrameOfOwningContent() const {
  return mOwnerContent ? mOwnerContent->GetPrimaryFrame() : nullptr;
}

Document* nsFrameLoader::GetOwnerDoc() const {
  return mOwnerContent ? mOwnerContent->OwnerDoc() : nullptr;
}

bool nsFrameLoader::IsRemoteFrame() {
  if (mIsRemoteFrame) {
    MOZ_ASSERT(!GetDocShell(), "Found a remote frame with a DocShell");
    return true;
  }
  return false;
}

RemoteBrowser* nsFrameLoader::GetRemoteBrowser() const {
  return mRemoteBrowser;
}

BrowserParent* nsFrameLoader::GetBrowserParent() const {
  if (!mRemoteBrowser) {
    return nullptr;
  }
  RefPtr<BrowserHost> browserHost = mRemoteBrowser->AsBrowserHost();
  if (!browserHost) {
    return nullptr;
  }
  return browserHost->GetActor();
}

BrowserBridgeChild* nsFrameLoader::GetBrowserBridgeChild() const {
  if (!mRemoteBrowser) {
    return nullptr;
  }
  RefPtr<BrowserBridgeHost> browserBridgeHost =
      mRemoteBrowser->AsBrowserBridgeHost();
  if (!browserBridgeHost) {
    return nullptr;
  }
  return browserBridgeHost->GetActor();
}

mozilla::layers::LayersId nsFrameLoader::GetLayersId() const {
  MOZ_ASSERT(mIsRemoteFrame);
  return mRemoteBrowser->GetLayersId();
}

void nsFrameLoader::ActivateRemoteFrame(ErrorResult& aRv) {
  auto* browserParent = GetBrowserParent();
  if (!browserParent) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  browserParent->Activate(nsFocusManager::GenerateFocusActionId());
}

void nsFrameLoader::DeactivateRemoteFrame(ErrorResult& aRv) {
  auto* browserParent = GetBrowserParent();
  if (!browserParent) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  browserParent->Deactivate(false, nsFocusManager::GenerateFocusActionId());
}

void nsFrameLoader::ActivateFrameEvent(const nsAString& aType, bool aCapture,
                                       ErrorResult& aRv) {
  auto* browserParent = GetBrowserParent();
  if (!browserParent) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool ok = browserParent->SendActivateFrameEvent(nsString(aType), aCapture);
  if (!ok) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
  }
}

nsresult nsFrameLoader::DoRemoteStaticClone(nsFrameLoader* aStaticCloneOf,
                                            nsIPrintSettings* aPrintSettings) {
  MOZ_ASSERT(aStaticCloneOf->IsRemoteFrame());
  MOZ_ASSERT(aPrintSettings);
  auto* cc = ContentChild::GetSingleton();
  if (!cc) {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    // TODO: Could possibly be implemented without too much effort.
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  BrowsingContext* bcToClone = aStaticCloneOf->GetBrowsingContext();
  if (NS_WARN_IF(!bcToClone)) {
    return NS_ERROR_UNEXPECTED;
  }
  BrowsingContext* bc = GetBrowsingContext();
  MOZ_DIAGNOSTIC_ASSERT(bc);
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return NS_ERROR_UNEXPECTED;
  }
  embedding::PrintData printData;
  nsresult rv =
      printSettingsSvc->SerializeToPrintData(aPrintSettings, &printData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  cc->SendCloneDocumentTreeInto(bcToClone, bc, printData);
  return NS_OK;
}

nsresult nsFrameLoader::FinishStaticClone(
    nsFrameLoader* aStaticCloneOf, nsIPrintSettings* aPrintSettings,
    bool* aOutHasInProcessPrintCallbacks) {
  MOZ_DIAGNOSTIC_ASSERT(
      !nsContentUtils::IsSafeToRunScript(),
      "A script blocker should be on the stack while FinishStaticClone is run");

  // NOTE: We don't check `aStaticCloneOf->IsDead()` here, as the nsFrameLoader
  // which we're a static clone of may be in the process of being destroyed. It
  // won't be fully destroyed when `FinishStaticClone` is called, as a script
  // blocker on our caller's stack is preventing it from becoming finalized.
  //
  // This is quite fragile, but is quite difficult to work around without
  // getting print-preview to stop re-cloning and replacing the previewed
  // document when changing layout.
  if (NS_WARN_IF(IsDead())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (aStaticCloneOf->IsRemoteFrame()) {
    return DoRemoteStaticClone(aStaticCloneOf, aPrintSettings);
  }

  nsIDocShell* origDocShell = aStaticCloneOf->GetDocShell();
  NS_ENSURE_STATE(origDocShell);

  nsCOMPtr<Document> doc = origDocShell->GetDocument();
  NS_ENSURE_STATE(doc);

  MaybeCreateDocShell();
  RefPtr<nsDocShell> docShell = GetDocShell();
  NS_ENSURE_STATE(docShell);

  nsCOMPtr<Document> kungFuDeathGrip = docShell->GetDocument();
  Unused << kungFuDeathGrip;

  nsCOMPtr<nsIContentViewer> viewer;
  docShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<Document> clonedDoc = doc->CreateStaticClone(
      docShell, viewer, aPrintSettings, aOutHasInProcessPrintCallbacks);

  return NS_OK;
}

bool nsFrameLoader::DoLoadMessageManagerScript(const nsAString& aURL,
                                               bool aRunInGlobalScope) {
  if (auto* browserParent = GetBrowserParent()) {
    return browserParent->SendLoadRemoteScript(nsString(aURL),
                                               aRunInGlobalScope);
  }
  RefPtr<InProcessBrowserChildMessageManager> browserChild =
      GetBrowserChildMessageManager();
  if (browserChild) {
    browserChild->LoadFrameScript(aURL, aRunInGlobalScope);
  }
  return true;
}

class nsAsyncMessageToChild : public nsSameProcessAsyncMessageBase,
                              public Runnable {
 public:
  explicit nsAsyncMessageToChild(nsFrameLoader* aFrameLoader)
      : nsSameProcessAsyncMessageBase(),
        mozilla::Runnable("nsAsyncMessageToChild"),
        mFrameLoader(aFrameLoader) {}

  NS_IMETHOD Run() override {
    InProcessBrowserChildMessageManager* browserChild =
        mFrameLoader->mChildMessageManager;
    // Since bug 1126089, messages can arrive even when the docShell is
    // destroyed. Here we make sure that those messages are not delivered.
    if (browserChild && browserChild->GetInnerManager() &&
        mFrameLoader->GetExistingDocShell()) {
      JS::Rooted<JSObject*> kungFuDeathGrip(dom::RootingCx(),
                                            browserChild->GetWrapper());
      ReceiveMessage(static_cast<EventTarget*>(browserChild), mFrameLoader,
                     browserChild->GetInnerManager());
    }
    return NS_OK;
  }
  RefPtr<nsFrameLoader> mFrameLoader;
};

nsresult nsFrameLoader::DoSendAsyncMessage(const nsAString& aMessage,
                                           StructuredCloneData& aData) {
  auto* browserParent = GetBrowserParent();
  if (browserParent) {
    ClonedMessageData data;
    ContentParent* cp = browserParent->Manager();
    if (!BuildClonedMessageDataForParent(cp, aData, data)) {
      MOZ_CRASH();
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    if (browserParent->SendAsyncMessage(nsString(aMessage), data)) {
      return NS_OK;
    } else {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mChildMessageManager) {
    RefPtr<nsAsyncMessageToChild> ev = new nsAsyncMessageToChild(this);
    nsresult rv = ev->Init(aMessage, aData);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return rv;
  }

  // We don't have any targets to send our asynchronous message to.
  return NS_ERROR_UNEXPECTED;
}

already_AddRefed<MessageSender> nsFrameLoader::GetMessageManager() {
  EnsureMessageManager();
  return do_AddRef(mMessageManager);
}

nsresult nsFrameLoader::EnsureMessageManager() {
  NS_ENSURE_STATE(mOwnerContent);

  if (mMessageManager) {
    return NS_OK;
  }

  if (!mIsTopLevelContent && !OwnerIsMozBrowserFrame() && !IsRemoteFrame() &&
      !(mOwnerContent->IsXULElement() &&
        mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::forcemessagemanager,
                                   nsGkAtoms::_true, eCaseMatters))) {
    return NS_OK;
  }

  RefPtr<nsGlobalWindowOuter> window =
      nsGlobalWindowOuter::Cast(GetOwnerDoc()->GetWindow());
  RefPtr<ChromeMessageBroadcaster> parentManager;

  if (window && window->IsChromeWindow()) {
    nsAutoString messagemanagergroup;
    if (mOwnerContent->IsXULElement() &&
        mOwnerContent->GetAttr(kNameSpaceID_None,
                               nsGkAtoms::messagemanagergroup,
                               messagemanagergroup)) {
      parentManager = window->GetGroupMessageManager(messagemanagergroup);
    }

    if (!parentManager) {
      parentManager = window->GetMessageManager();
    }
  } else {
    parentManager = nsFrameMessageManager::GetGlobalMessageManager();
  }

  mMessageManager = new ChromeMessageSender(parentManager);
  if (!IsRemoteFrame()) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      return rv;
    }
    MOZ_ASSERT(GetDocShell(),
               "MaybeCreateDocShell succeeded, but null docShell");
    if (!GetDocShell()) {
      return NS_ERROR_FAILURE;
    }
    mChildMessageManager = InProcessBrowserChildMessageManager::Create(
        GetDocShell(), mOwnerContent, mMessageManager);
    NS_ENSURE_TRUE(mChildMessageManager, NS_ERROR_UNEXPECTED);

    // Set up a TabListener for sessionStore
    if constexpr (SessionStoreUtils::NATIVE_LISTENER) {
      if (XRE_IsParentProcess()) {
        mSessionStoreListener = new TabListener(GetDocShell(), mOwnerContent);
        rv = mSessionStoreListener->Init();
        NS_ENSURE_SUCCESS(rv, rv);

        mSessionStoreChangeListener =
            SessionStoreChangeListener::Create(GetExtantBrowsingContext());
      }
    }
  }
  return NS_OK;
}

nsresult nsFrameLoader::ReallyLoadFrameScripts() {
  nsresult rv = EnsureMessageManager();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (mMessageManager) {
    mMessageManager->InitWithCallback(this);
  }
  return NS_OK;
}

already_AddRefed<Element> nsFrameLoader::GetOwnerElement() {
  return do_AddRef(mOwnerContent);
}

void nsFrameLoader::SetDetachedSubdocFrame(nsIFrame* aDetachedFrame,
                                           Document* aContainerDoc) {
  mDetachedSubdocFrame = aDetachedFrame;
  mContainerDocWhileDetached = aContainerDoc;
}

nsIFrame* nsFrameLoader::GetDetachedSubdocFrame(
    Document** aContainerDoc) const {
  NS_IF_ADDREF(*aContainerDoc = mContainerDocWhileDetached);
  return mDetachedSubdocFrame.GetFrame();
}

void nsFrameLoader::ApplySandboxFlags(uint32_t sandboxFlags) {
  // If our BrowsingContext doesn't exist yet, it means we haven't been
  // initialized yet. This method will be called again once we're initialized
  // from MaybeCreateDocShell. <iframe> BrowsingContexts are never created as
  // initially remote, so we don't need to worry about updating sandbox flags
  // for an uninitialized initially-remote iframe.
  BrowsingContext* context = GetExtantBrowsingContext();
  if (!context) {
    MOZ_ASSERT(!IsRemoteFrame(),
               "cannot apply sandbox flags to an uninitialized "
               "initially-remote frame");
    return;
  }

  uint32_t parentSandboxFlags = mOwnerContent->OwnerDoc()->GetSandboxFlags();

  // The child can only add restrictions, never remove them.
  sandboxFlags |= parentSandboxFlags;

  MOZ_ALWAYS_SUCCEEDS(context->SetSandboxFlags(sandboxFlags));
}

/* virtual */
void nsFrameLoader::AttributeChanged(mozilla::dom::Element* aElement,
                                     int32_t aNameSpaceID, nsAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aOldValue) {
  MOZ_ASSERT(mObservingOwnerContent);

  if (aElement != mOwnerContent) {
    return;
  }

  if (aNameSpaceID != kNameSpaceID_None ||
      (aAttribute != TypeAttrName(aElement) &&
       aAttribute != nsGkAtoms::primary)) {
    return;
  }

  // Note: This logic duplicates a lot of logic in
  // MaybeCreateDocshell.  We should fix that.

  // Notify our enclosing chrome that our type has changed.  We only do this
  // if our parent is chrome, since in all other cases we're random content
  // subframes and the treeowner shouldn't worry about us.
  if (!GetDocShell()) {
    MaybeUpdatePrimaryBrowserParent(eBrowserParentChanged);
    return;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  GetDocShell()->GetInProcessParent(getter_AddRefs(parentItem));
  if (!parentItem) {
    return;
  }

  if (parentItem->ItemType() != nsIDocShellTreeItem::typeChrome) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  if (!parentTreeOwner) {
    return;
  }

  bool is_primary = aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                                          nsGkAtoms::_true, eIgnoreCase);

#ifdef MOZ_XUL
  // when a content panel is no longer primary, hide any open popups it may have
  if (!is_primary) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->HidePopupsInDocShell(GetDocShell());
    }
  }
#endif

  parentTreeOwner->ContentShellRemoved(GetDocShell());
  if (aElement->AttrValueIs(kNameSpaceID_None, TypeAttrName(aElement),
                            nsGkAtoms::content, eIgnoreCase)) {
    parentTreeOwner->ContentShellAdded(GetDocShell(), is_primary);
  }
}

void nsFrameLoader::RequestUpdatePosition(ErrorResult& aRv) {
  if (auto* browserParent = GetBrowserParent()) {
    nsresult rv = browserParent->UpdatePosition();

    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
    }
  }
}

already_AddRefed<Promise> nsFrameLoader::RequestTabStateFlush(
    ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise =
      Promise::Create(GetOwnerDoc()->GetOwnerGlobal(), aRv);

  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<BrowsingContext> context = GetExtantBrowsingContext();
  if (!context) {
    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  if (mSessionStoreListener) {
    context->FlushSessionStore();
    mSessionStoreListener->ForceFlushFromParent();
    context->Canonical()->UpdateSessionStoreSessionStorage(
        [promise]() { promise->MaybeResolveWithUndefined(); });

    return promise.forget();
  }

  using FlushPromise = ContentParent::FlushTabStatePromise;
  nsTArray<RefPtr<FlushPromise>> flushPromises;
  context->Group()->EachParent([&](ContentParent* aParent) {
    if (aParent->CanSend()) {
      flushPromises.AppendElement(aParent->SendFlushTabState(context));
    }
  });

  RefPtr<FlushPromise::AllPromiseType> flushPromise =
      FlushPromise::All(GetCurrentSerialEventTarget(), flushPromises);

  context->Canonical()->UpdateSessionStoreSessionStorage([flushPromise,
                                                          promise]() {
    flushPromise->Then(GetCurrentSerialEventTarget(), __func__,
                       [promise]() { promise->MaybeResolveWithUndefined(); });
  });

  return promise.forget();
}

void nsFrameLoader::RequestFinalTabStateFlush() {
  BrowsingContext* context = GetExtantBrowsingContext();
  if (!context || !context->IsTop() || context->Canonical()->IsReplaced()) {
    return;
  }

  RefPtr<CanonicalBrowsingContext> canonical = context->Canonical();
  RefPtr<WindowGlobalParent> wgp = canonical->GetCurrentWindowGlobal();
  RefPtr<Element> embedder = context->GetEmbedderElement();

  if (mSessionStoreListener) {
    context->FlushSessionStore();
    mSessionStoreListener->ForceFlushFromParent();

    canonical->ClearPermanentKey();
    if (wgp) {
      wgp->NotifySessionStoreUpdatesComplete(embedder);
    }

    return;
  }

  using FlushPromise = ContentParent::FlushTabStatePromise;
  nsTArray<RefPtr<FlushPromise>> flushPromises;
  context->Group()->EachParent([&](ContentParent* aParent) {
    if (aParent->CanSend()) {
      flushPromises.AppendElement(aParent->SendFlushTabState(context));
    }
  });

  FlushPromise::All(GetCurrentSerialEventTarget(), flushPromises)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [canonical = RefPtr{canonical}, wgp, embedder]() {
               if (canonical) {
                 canonical->ClearPermanentKey();
               }
               if (wgp) {
                 wgp->NotifySessionStoreUpdatesComplete(embedder);
               }
             });
}

void nsFrameLoader::RequestEpochUpdate(uint32_t aEpoch) {
  BrowsingContext* context = GetExtantBrowsingContext();
  if (context) {
    BrowsingContext* top = context->Top();
    Unused << top->SetSessionStoreEpoch(aEpoch);
  }

  if (mSessionStoreListener) {
    mSessionStoreListener->SetEpoch(aEpoch);
    return;
  }

  // If remote browsing (e10s), handle this with the BrowserParent.
  if (auto* browserParent = GetBrowserParent()) {
    Unused << browserParent->SendUpdateEpoch(aEpoch);
  }
}

void nsFrameLoader::RequestSHistoryUpdate() {
  if (mSessionStoreListener) {
    mSessionStoreListener->UpdateSHistoryChanges();
    return;
  }

  // If remote browsing (e10s), handle this with the BrowserParent.
  if (auto* browserParent = GetBrowserParent()) {
    Unused << browserParent->SendUpdateSHistory();
  }
}

already_AddRefed<Promise> nsFrameLoader::PrintPreview(
    nsIPrintSettings* aPrintSettings, BrowsingContext* aSourceBrowsingContext,
    ErrorResult& aRv) {
  auto* ownerDoc = GetOwnerDoc();
  if (!ownerDoc) {
    aRv.ThrowNotSupportedError("No owner document");
    return nullptr;
  }
  RefPtr<Promise> promise = Promise::Create(ownerDoc->GetOwnerGlobal(), aRv);
  if (!promise) {
    return nullptr;
  }

#ifndef NS_PRINTING
  promise->MaybeRejectWithNotSupportedError("Build does not support printing");
  return promise.forget();
#else
  auto resolve = [promise](PrintPreviewResultInfo aInfo) {
    using Orientation = dom::PrintPreviewOrientation;
    if (aInfo.sheetCount() > 0) {
      PrintPreviewSuccessInfo info;
      info.mSheetCount = aInfo.sheetCount();
      info.mTotalPageCount = aInfo.totalPageCount();
      info.mHasSelection = aInfo.hasSelection();
      info.mHasSelfSelection = aInfo.hasSelfSelection();
      info.mIsEmpty = aInfo.isEmpty();
      if (aInfo.printLandscape()) {
        info.mOrientation = aInfo.printLandscape().value()
                                ? Orientation::Landscape
                                : Orientation::Portrait;
      } else {
        MOZ_ASSERT(info.mOrientation == Orientation::Unspecified);
      }
      promise->MaybeResolve(info);
    } else {
      promise->MaybeRejectWithUnknownError("Print preview failed");
    }
  };

  if (auto* browserParent = GetBrowserParent()) {
    nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
        do_GetService("@mozilla.org/gfx/printsettings-service;1");
    if (!printSettingsSvc) {
      promise->MaybeRejectWithNotSupportedError("No nsIPrintSettingsService");
      return promise.forget();
    }

    embedding::PrintData printData;
    nsresult rv =
        printSettingsSvc->SerializeToPrintData(aPrintSettings, &printData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      promise->MaybeReject(ErrorResult(rv));
      return promise.forget();
    }

    browserParent->SendPrintPreview(printData, aSourceBrowsingContext)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__, std::move(resolve),
            [promise](const mozilla::ipc::ResponseRejectReason) {
              promise->MaybeRejectWithUnknownError("Print preview IPC failed");
            });

    return promise.forget();
  }

  RefPtr<nsGlobalWindowOuter> sourceWindow;
  if (aSourceBrowsingContext) {
    sourceWindow =
        nsGlobalWindowOuter::Cast(aSourceBrowsingContext->GetDOMWindow());
  } else {
    nsDocShell* ourDocshell = GetExistingDocShell();
    if (NS_WARN_IF(!ourDocshell)) {
      promise->MaybeRejectWithNotSupportedError("No print preview docShell");
      return promise.forget();
    }
    sourceWindow = nsGlobalWindowOuter::Cast(ourDocshell->GetWindow());
  }
  if (NS_WARN_IF(!sourceWindow)) {
    promise->MaybeRejectWithNotSupportedError("No print preview source window");
    return promise.forget();
  }

  nsCOMPtr<nsIDocShell> docShellToCloneInto = nullptr;
  if (aSourceBrowsingContext) {
    // We're going to call `Print()` below on a window that is not our own,
    // which happens when we are creating a new print preview document instead
    // of just applying a settings change to the existing PP document.  In this
    // case we need to explicitly pass our docShell as the docShell to clone
    // into.
    docShellToCloneInto = GetExistingDocShell();
    if (NS_WARN_IF(!docShellToCloneInto)) {
      promise->MaybeRejectWithNotSupportedError("No print preview docShell");
      return promise.forget();
    }

    // We need to make sure we're displayed so that the view tree ends up right.
    RefPtr<BrowsingContext> bc = docShellToCloneInto->GetBrowsingContext();
    if (NS_WARN_IF(!bc)) {
      promise->MaybeRejectWithNotSupportedError(
          "No print preview browsing context");
      return promise.forget();
    }

    RefPtr<Element> embedder = bc->GetEmbedderElement();
    if (NS_WARN_IF(!embedder)) {
      promise->MaybeRejectWithNotSupportedError(
          "Trying to clone into a frameloader with no element?");
      return promise.forget();
    }

    nsIFrame* frame = embedder->GetPrimaryFrame(FlushType::Frames);
    if (NS_WARN_IF(!frame)) {
      promise->MaybeRejectWithNotSupportedError("Frame is not being displayed");
      return promise.forget();
    }
  }

  // Unfortunately we can't pass `resolve` directly here because IPDL, for now,
  // unfortunately generates slightly different parameter types for functions
  // taking PrintPreviewResultInfo in PBrowserParent vs. PBrowserChild.
  ErrorResult rv;
  sourceWindow->Print(
      aPrintSettings,
      /* aListener = */ nullptr, docShellToCloneInto,
      nsGlobalWindowOuter::IsPreview::Yes,
      nsGlobalWindowOuter::IsForWindowDotPrint::No,
      [resolve](const PrintPreviewResultInfo& aInfo) { resolve(aInfo); }, rv);
  if (NS_WARN_IF(rv.Failed())) {
    promise->MaybeReject(std::move(rv));
  }

  return promise.forget();
#endif
}

void nsFrameLoader::ExitPrintPreview() {
#ifdef NS_PRINTING
  if (auto* browserParent = GetBrowserParent()) {
    Unused << browserParent->SendExitPrintPreview();
    return;
  }
  if (NS_WARN_IF(!GetExistingDocShell())) {
    return;
  }
  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
      do_GetInterface(ToSupports(GetExistingDocShell()->GetWindow()));
  if (NS_WARN_IF(!webBrowserPrint)) {
    return;
  }
  webBrowserPrint->ExitPrintPreview();
#endif
}

already_AddRefed<nsIRemoteTab> nsFrameLoader::GetRemoteTab() {
  if (!mRemoteBrowser) {
    return nullptr;
  }
  if (auto* browserHost = mRemoteBrowser->AsBrowserHost()) {
    return do_AddRef(browserHost);
  }
  return nullptr;
}

already_AddRefed<nsILoadContext> nsFrameLoader::LoadContext() {
  return do_AddRef(GetBrowsingContext());
}

BrowsingContext* nsFrameLoader::GetBrowsingContext() {
  if (!mInitialized) {
    if (IsRemoteFrame()) {
      Unused << EnsureRemoteBrowser();
    } else if (mOwnerContent) {
      Unused << MaybeCreateDocShell();
    }
  }
  MOZ_ASSERT(mInitialized);
  return GetExtantBrowsingContext();
}

BrowsingContext* nsFrameLoader::GetExtantBrowsingContext() {
  if (!mPendingBrowsingContext) {
    // If mPendingBrowsingContext is null then the frame loader is being
    // destroyed (nsFrameLoader::DestroyDocShell was called), so return null
    // here in that case.
    return nullptr;
  }

  if (!mInitialized || !mPendingBrowsingContext->EverAttached()) {
    // Don't return the pending BrowsingContext until this nsFrameLoader has
    // been initialized, and the BC was attached.
    return nullptr;
  }

  return mPendingBrowsingContext;
}

void nsFrameLoader::InitializeBrowserAPI() {
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }

  nsresult rv = EnsureMessageManager();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mMessageManager->LoadFrameScript(
      u"chrome://global/content/BrowserElementChild.js"_ns,
      /* allowDelayedLoad = */ true,
      /* aRunInGlobalScope */ true, IgnoreErrors());

  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->InitializeBrowserAPI();
  }
}

void nsFrameLoader::DestroyBrowserFrameScripts() {
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->DestroyBrowserFrameScripts();
  }
}

void nsFrameLoader::StartPersistence(
    BrowsingContext* aContext, nsIWebBrowserPersistDocumentReceiver* aRecv,
    ErrorResult& aRv) {
  MOZ_ASSERT(aRecv);
  RefPtr<BrowsingContext> context = aContext ? aContext : GetBrowsingContext();

  if (!context || !context->IsInSubtreeOf(GetBrowsingContext())) {
    aRecv->OnError(NS_ERROR_NO_CONTENT);
    return;
  }

  if (!context->GetDocShell() && XRE_IsParentProcess()) {
    CanonicalBrowsingContext* canonical =
        CanonicalBrowsingContext::Cast(context);
    if (!canonical->GetCurrentWindowGlobal()) {
      aRecv->OnError(NS_ERROR_NO_CONTENT);
      return;
    }
    RefPtr<BrowserParent> browserParent =
        canonical->GetCurrentWindowGlobal()->GetBrowserParent();
    browserParent->StartPersistence(canonical, aRecv, aRv);
    return;
  }

  nsCOMPtr<Document> foundDoc = context->GetDocument();

  if (!foundDoc) {
    aRecv->OnError(NS_ERROR_NO_CONTENT);
  } else {
    nsCOMPtr<nsIWebBrowserPersistDocument> pdoc =
        new mozilla::WebBrowserPersistLocalDocument(foundDoc);
    aRecv->OnDocumentReady(pdoc);
  }
}

void nsFrameLoader::MaybeUpdatePrimaryBrowserParent(
    BrowserParentChange aChange) {
  if (!mOwnerContent || !mRemoteBrowser) {
    return;
  }

  RefPtr<BrowserHost> browserHost = mRemoteBrowser->AsBrowserHost();
  if (!browserHost) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
  if (!docShell) {
    return;
  }

  BrowsingContext* browsingContext = docShell->GetBrowsingContext();
  if (!browsingContext->IsChrome()) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  if (!parentTreeOwner) {
    return;
  }

  if (!mObservingOwnerContent) {
    mOwnerContent->AddMutationObserver(this);
    mObservingOwnerContent = true;
  }

  parentTreeOwner->RemoteTabRemoved(browserHost);
  if (aChange == eBrowserParentChanged) {
    bool isPrimary = mOwnerContent->AttrValueIs(
        kNameSpaceID_None, nsGkAtoms::primary, nsGkAtoms::_true, eIgnoreCase);
    parentTreeOwner->RemoteTabAdded(browserHost, isPrimary);
  }
}

nsresult nsFrameLoader::GetNewTabContext(MutableTabContext* aTabContext,
                                         nsIURI* aURI) {
  nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
  nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(docShell);
  NS_ENSURE_STATE(parentContext);

  MOZ_ASSERT(mPendingBrowsingContext->EverAttached());

  UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
  uint64_t chromeOuterWindowID = 0;

  Document* doc = mOwnerContent->OwnerDoc();
  if (doc) {
    nsCOMPtr<nsPIWindowRoot> root = nsContentUtils::GetWindowRoot(doc);
    if (root) {
      showFocusRings = root->ShowFocusRings() ? UIStateChangeType_Set
                                              : UIStateChangeType_Clear;

      nsPIDOMWindowOuter* outerWin = root->GetWindow();
      if (outerWin) {
        chromeOuterWindowID = outerWin->WindowID();
      }
    }
  }

  uint32_t maxTouchPoints = BrowserParent::GetMaxTouchPoints(mOwnerContent);

  bool tabContextUpdated = aTabContext->SetTabContext(
      chromeOuterWindowID, showFocusRings, maxTouchPoints);
  NS_ENSURE_STATE(tabContextUpdated);

  return NS_OK;
}

nsresult nsFrameLoader::PopulateOriginContextIdsFromAttributes(
    OriginAttributes& aAttr) {
  // Only XUL or mozbrowser frames are allowed to set context IDs
  uint32_t namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID != kNameSpaceID_XUL && !OwnerIsMozBrowserFrame()) {
    return NS_OK;
  }

  nsAutoString attributeValue;
  if (aAttr.mUserContextId ==
          nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID &&
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usercontextid,
                             attributeValue) &&
      !attributeValue.IsEmpty()) {
    nsresult rv;
    aAttr.mUserContextId = attributeValue.ToInteger(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aAttr.mGeckoViewSessionContextId.IsEmpty() &&
      mOwnerContent->GetAttr(kNameSpaceID_None,
                             nsGkAtoms::geckoViewSessionContextId,
                             attributeValue) &&
      !attributeValue.IsEmpty()) {
    // XXX: Should we check the format from `GeckoViewNavigation.jsm` here?
    aAttr.mGeckoViewSessionContextId = attributeValue;
  }

  return NS_OK;
}

ProcessMessageManager* nsFrameLoader::GetProcessMessageManager() const {
  if (auto* browserParent = GetBrowserParent()) {
    return browserParent->Manager()->GetMessageManager();
  }
  return nullptr;
};

JSObject* nsFrameLoader::WrapObject(JSContext* cx,
                                    JS::Handle<JSObject*> aGivenProto) {
  JS::RootedObject result(cx);
  FrameLoader_Binding::Wrap(cx, this, this, aGivenProto, &result);
  return result;
}

void nsFrameLoader::SetWillChangeProcess() {
  mWillChangeProcess = true;

  if (IsRemoteFrame()) {
    if (auto* browserParent = GetBrowserParent()) {
      if (auto* bc = CanonicalBrowsingContext::Cast(mPendingBrowsingContext);
          bc && bc->EverAttached()) {
        bc->StartUnloadingHost(browserParent->Manager()->ChildID());
        bc->SetCurrentBrowserParent(nullptr);
      }
      // OOP Browser - Go directly over Browser Parent
      Unused << browserParent->SendWillChangeProcess();
    } else if (auto* browserBridgeChild = GetBrowserBridgeChild()) {
      // OOP IFrame - Through Browser Bridge Parent, set on browser child
      Unused << browserBridgeChild->SendWillChangeProcess();
    }
    return;
  }

  // In process
  RefPtr<nsDocShell> docshell = GetDocShell();
  MOZ_ASSERT(docshell);
  docshell->SetWillChangeProcess();
}

static mozilla::Result<bool, nsresult> DidBuildIDChange() {
  nsresult rv;
  nsCOMPtr<nsIFile> file;

  rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(file));
  MOZ_TRY(rv);

  rv = file->AppendNative("platform.ini"_ns);
  MOZ_TRY(rv);

  nsCOMPtr<nsIINIParserFactory> iniFactory =
      do_GetService("@mozilla.org/xpcom/ini-parser-factory;1", &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIINIParser> parser;
  rv = iniFactory->CreateINIParser(file, getter_AddRefs(parser));
  MOZ_TRY(rv);

  nsAutoCString installedBuildID;
  rv = parser->GetString("Build"_ns, "BuildID"_ns, installedBuildID);
  MOZ_TRY(rv);

  nsDependentCString runningBuildID(PlatformBuildID());
  return (installedBuildID != runningBuildID);
}

void nsFrameLoader::MaybeNotifyCrashed(BrowsingContext* aBrowsingContext,
                                       ContentParentId aChildID,
                                       mozilla::ipc::MessageChannel* aChannel) {
  if (mTabProcessCrashFired) {
    return;
  }

  if (mPendingBrowsingContext == aBrowsingContext) {
    mTabProcessCrashFired = true;
  }

  // Fire the crashed observer notification.
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (!os) {
    return;
  }

  os->NotifyObservers(ToSupports(this), "oop-frameloader-crashed", nullptr);

  // Check our owner element still references us. If it's moved, on, events
  // don't need to be fired.
  RefPtr<nsFrameLoaderOwner> owner = do_QueryObject(mOwnerContent);
  if (!owner) {
    return;
  }

  RefPtr<nsFrameLoader> currentFrameLoader = owner->GetFrameLoader();
  if (currentFrameLoader != this) {
    return;
  }

#if defined(MOZ_TELEMETRY_REPORTING)
  bool sendTelemetry = false;
#endif  // defined(MOZ_TELEMETRY_REPORTING)

  // Fire the actual crashed event.
  nsString eventName;
  if (aChannel && !aChannel->DoBuildIDsMatch()) {
    auto changedOrError = DidBuildIDChange();
    if (changedOrError.isErr()) {
      NS_WARNING("Error while checking buildid mismatch");
      eventName = u"oop-browser-buildid-mismatch"_ns;
    } else {
      bool aChanged = changedOrError.unwrap();
      if (aChanged) {
        NS_WARNING("True build ID mismatch");
        eventName = u"oop-browser-buildid-mismatch"_ns;
      } else {
        NS_WARNING("build ID mismatch false alarm");
        eventName = u"oop-browser-crashed"_ns;
#if defined(MOZ_TELEMETRY_REPORTING)
        sendTelemetry = true;
#endif  // defined(MOZ_TELEMETRY_REPORTING)
      }
    }
  } else {
    NS_WARNING("No build ID mismatch");
    eventName = u"oop-browser-crashed"_ns;
  }

#if defined(MOZ_TELEMETRY_REPORTING)
  if (sendTelemetry) {
    Telemetry::ScalarAdd(
        Telemetry::ScalarID::DOM_CONTENTPROCESS_BUILDID_MISMATCH_FALSE_POSITIVE,
        1);
  }
#endif  // defined(MOZ_TELEMETRY_REPORTING)

  FrameCrashedEventInit init;
  init.mBubbles = true;
  init.mCancelable = true;
  if (aBrowsingContext) {
    init.mBrowsingContextId = aBrowsingContext->Id();
    init.mIsTopFrame = aBrowsingContext->IsTop();
    init.mChildID = aChildID;
  }

  RefPtr<FrameCrashedEvent> event = FrameCrashedEvent::Constructor(
      mOwnerContent->OwnerDoc(), eventName, init);
  event->SetTrusted(true);
  EventDispatcher::DispatchDOMEvent(mOwnerContent, nullptr, event, nullptr,
                                    nullptr);
}

bool nsFrameLoader::EnsureBrowsingContextAttached() {
  nsresult rv;

  Document* parentDoc = mOwnerContent->OwnerDoc();
  MOZ_ASSERT(parentDoc);
  BrowsingContext* parentContext = parentDoc->GetBrowsingContext();
  MOZ_ASSERT(parentContext);

  // Inherit the `use` flags from our parent BrowsingContext.
  bool usePrivateBrowsing = parentContext->UsePrivateBrowsing();
  bool useRemoteSubframes = parentContext->UseRemoteSubframes();
  bool useRemoteTabs = parentContext->UseRemoteTabs();

  // Determine the exact OriginAttributes which should be used for our
  // BrowsingContext. This will be used to initialize OriginAttributes if the
  // BrowsingContext has not already been created.
  OriginAttributes attrs;
  if (mPendingBrowsingContext->IsContent()) {
    if (mPendingBrowsingContext->GetParent()) {
      MOZ_ASSERT(mPendingBrowsingContext->GetParent() == parentContext);
      parentContext->GetOriginAttributes(attrs);
    }

    // Inherit the `mFirstPartyDomain` flag from our parent document's result
    // principal, if it was set.
    if (parentContext->IsContent() &&
        !parentDoc->NodePrincipal()->IsSystemPrincipal() &&
        !OwnerIsMozBrowserFrame()) {
      OriginAttributes docAttrs =
          parentDoc->NodePrincipal()->OriginAttributesRef();
      // We only want to inherit firstPartyDomain here, other attributes should
      // be constant.
      MOZ_ASSERT(attrs.EqualsIgnoringFPD(docAttrs));
      attrs.mFirstPartyDomain = docAttrs.mFirstPartyDomain;
    }

    // Inherit the PrivateBrowsing flag across content/chrome boundaries.
    attrs.SyncAttributesWithPrivateBrowsing(usePrivateBrowsing);

    // A <browser> element may have overridden userContextId or
    // geckoViewUserContextId.
    rv = PopulateOriginContextIdsFromAttributes(attrs);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    // <iframe mozbrowser> is allowed to set `mozprivatebrowsing` to
    // force-enable private browsing.
    if (OwnerIsMozBrowserFrame()) {
      if (mOwnerContent->HasAttr(kNameSpaceID_None,
                                 nsGkAtoms::mozprivatebrowsing)) {
        attrs.SyncAttributesWithPrivateBrowsing(true);
        usePrivateBrowsing = true;
      }
    }
  }

  // If we've already been attached, return.
  if (mPendingBrowsingContext->EverAttached()) {
    MOZ_DIAGNOSTIC_ASSERT(mPendingBrowsingContext->UsePrivateBrowsing() ==
                          usePrivateBrowsing);
    MOZ_DIAGNOSTIC_ASSERT(mPendingBrowsingContext->UseRemoteTabs() ==
                          useRemoteTabs);
    MOZ_DIAGNOSTIC_ASSERT(mPendingBrowsingContext->UseRemoteSubframes() ==
                          useRemoteSubframes);
    // Don't assert that our OriginAttributes match, as we could have different
    // OriginAttributes in the case where we were opened using window.open.
    return true;
  }

  // Initialize non-synced OriginAttributes and related fields.
  rv = mPendingBrowsingContext->SetOriginAttributes(attrs);
  NS_ENSURE_SUCCESS(rv, false);
  rv = mPendingBrowsingContext->SetUsePrivateBrowsing(usePrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, false);
  rv = mPendingBrowsingContext->SetRemoteTabs(useRemoteTabs);
  NS_ENSURE_SUCCESS(rv, false);
  rv = mPendingBrowsingContext->SetRemoteSubframes(useRemoteSubframes);
  NS_ENSURE_SUCCESS(rv, false);

  // Finish attaching.
  mPendingBrowsingContext->EnsureAttached();
  return true;
}

void nsFrameLoader::InvokeBrowsingContextReadyCallback() {
  if (mOpenWindowInfo) {
    if (RefPtr<nsIBrowsingContextReadyCallback> callback =
            mOpenWindowInfo->BrowsingContextReadyCallback()) {
      callback->BrowsingContextReady(mPendingBrowsingContext);
    }
  }
}

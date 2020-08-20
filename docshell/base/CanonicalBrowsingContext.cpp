/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CanonicalBrowsingContext.h"

#include "mozilla/EventForwards.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/MediaController.h"
#include "mozilla/dom/MediaControlService.h"
#include "mozilla/dom/ContentPlaybackController.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "mozilla/NullPrincipal.h"
#include "nsIWebNavigation.h"
#include "mozilla/MozPromiseInlines.h"
#include "nsGlobalWindowOuter.h"
#include "nsIWebBrowserChrome.h"
#include "nsNetUtil.h"
#include "nsSHistory.h"
#include "nsSecureBrowserUI.h"
#include "nsQueryObject.h"
#include "nsBrowserStatusFilter.h"
#include "nsIBrowser.h"

using namespace mozilla::ipc;

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

CanonicalBrowsingContext::CanonicalBrowsingContext(WindowContext* aParentWindow,
                                                   BrowsingContextGroup* aGroup,
                                                   uint64_t aBrowsingContextId,
                                                   uint64_t aOwnerProcessId,
                                                   uint64_t aEmbedderProcessId,
                                                   BrowsingContext::Type aType,
                                                   FieldValues&& aInit)
    : BrowsingContext(aParentWindow, aGroup, aBrowsingContextId, aType,
                      std::move(aInit)),
      mProcessId(aOwnerProcessId),
      mEmbedderProcessId(aEmbedderProcessId) {
  // You are only ever allowed to create CanonicalBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

/* static */
already_AddRefed<CanonicalBrowsingContext> CanonicalBrowsingContext::Get(
    uint64_t aId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<CanonicalBrowsingContext>();
}

/* static */
CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<CanonicalBrowsingContext*>(aContext);
}

/* static */
const CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    const BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const CanonicalBrowsingContext*>(aContext);
}

already_AddRefed<CanonicalBrowsingContext> CanonicalBrowsingContext::Cast(
    already_AddRefed<BrowsingContext>&& aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return aContext.downcast<CanonicalBrowsingContext>();
}

ContentParent* CanonicalBrowsingContext::GetContentParent() const {
  if (mProcessId == 0) {
    return nullptr;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  return cpm->GetContentProcessById(ContentParentId(mProcessId));
}

void CanonicalBrowsingContext::GetCurrentRemoteType(nsACString& aRemoteType,
                                                    ErrorResult& aRv) const {
  // If we're in the parent process, dump out the void string.
  if (mProcessId == 0) {
    aRemoteType = NOT_REMOTE_TYPE;
    return;
  }

  ContentParent* cp = GetContentParent();
  if (!cp) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRemoteType = cp->GetRemoteType();
}

void CanonicalBrowsingContext::SetOwnerProcessId(uint64_t aProcessId) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("SetOwnerProcessId for 0x%08" PRIx64 " (0x%08" PRIx64
           " -> 0x%08" PRIx64 ")",
           Id(), mProcessId, aProcessId));

  mProcessId = aProcessId;
}

nsISecureBrowserUI* CanonicalBrowsingContext::GetSecureBrowserUI() {
  if (!IsTop()) {
    return nullptr;
  }
  if (!mSecureBrowserUI) {
    mSecureBrowserUI = new nsSecureBrowserUI(this);
  }
  return mSecureBrowserUI;
}

void CanonicalBrowsingContext::MaybeAddAsProgressListener(
    nsIWebProgress* aWebProgress) {
  if (!GetWebProgress()) {
    return;
  }
  if (!mStatusFilter) {
    mStatusFilter = new nsBrowserStatusFilter();
    mStatusFilter->AddProgressListener(GetWebProgress(),
                                       nsIWebProgress::NOTIFY_ALL);
  }
  aWebProgress->AddProgressListener(mStatusFilter, nsIWebProgress::NOTIFY_ALL);
}

void CanonicalBrowsingContext::ReplacedBy(
    CanonicalBrowsingContext* aNewContext) {
  MOZ_ASSERT(!aNewContext->EverAttached());
  if (mStatusFilter) {
    mStatusFilter->RemoveProgressListener(mWebProgress);
    mStatusFilter = nullptr;
  }
  aNewContext->mWebProgress = std::move(mWebProgress);
  aNewContext->mFields.SetWithoutSyncing<IDX_BrowserId>(GetBrowserId());

  if (mSessionHistory) {
    mSessionHistory->SetBrowsingContext(aNewContext);
    mSessionHistory.swap(aNewContext->mSessionHistory);
  }

  MOZ_ASSERT(aNewContext->mLoadingEntries.IsEmpty());
  mLoadingEntries.SwapElements(aNewContext->mLoadingEntries);
  MOZ_ASSERT(!aNewContext->mActiveEntry);
  mActiveEntry.swap(aNewContext->mActiveEntry);
}

void CanonicalBrowsingContext::
    UpdateSecurityStateForLocationOrMixedContentChange() {
  if (mSecureBrowserUI) {
    mSecureBrowserUI->UpdateForLocationOrMixedContentChange();
  }
}

void CanonicalBrowsingContext::SetInFlightProcessId(uint64_t aProcessId) {
  MOZ_ASSERT(aProcessId);
  mInFlightProcessId = aProcessId;
}

void CanonicalBrowsingContext::ClearInFlightProcessId(uint64_t aProcessId) {
  MOZ_ASSERT(aProcessId);
  if (mInFlightProcessId == aProcessId) {
    mInFlightProcessId = 0;
  }
}

void CanonicalBrowsingContext::GetWindowGlobals(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindows) {
  aWindows.SetCapacity(GetWindowContexts().Length());
  for (auto& window : GetWindowContexts()) {
    aWindows.AppendElement(static_cast<WindowGlobalParent*>(window.get()));
  }
}

WindowGlobalParent* CanonicalBrowsingContext::GetCurrentWindowGlobal() const {
  return static_cast<WindowGlobalParent*>(GetCurrentWindowContext());
}

WindowGlobalParent* CanonicalBrowsingContext::GetParentWindowContext() {
  return static_cast<WindowGlobalParent*>(
      BrowsingContext::GetParentWindowContext());
}

WindowGlobalParent* CanonicalBrowsingContext::GetTopWindowContext() {
  return static_cast<WindowGlobalParent*>(
      BrowsingContext::GetTopWindowContext());
}

already_AddRefed<nsIWidget>
CanonicalBrowsingContext::GetParentProcessWidgetContaining() {
  // If our document is loaded in-process, such as chrome documents, get the
  // widget directly from our outer window. Otherwise, try to get the widget
  // from the toplevel content's browser's element.
  nsCOMPtr<nsIWidget> widget;
  if (nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(GetDOMWindow())) {
    widget = window->GetNearestWidget();
  } else if (Element* topEmbedder = Top()->GetEmbedderElement()) {
    widget = nsContentUtils::WidgetForContent(topEmbedder);
    if (!widget) {
      widget = nsContentUtils::WidgetForDocument(topEmbedder->OwnerDoc());
    }
  }

  if (widget) {
    widget = widget->GetTopLevelWidget();
  }

  return widget.forget();
}

already_AddRefed<WindowGlobalParent>
CanonicalBrowsingContext::GetEmbedderWindowGlobal() const {
  uint64_t windowId = GetEmbedderInnerWindowId();
  if (windowId == 0) {
    return nullptr;
  }

  return WindowGlobalParent::GetByInnerWindowId(windowId);
}

already_AddRefed<CanonicalBrowsingContext>
CanonicalBrowsingContext::GetParentCrossChromeBoundary() {
  if (GetParent()) {
    return do_AddRef(Cast(GetParent()));
  }
  if (GetEmbedderElement()) {
    return do_AddRef(
        Cast(GetEmbedderElement()->OwnerDoc()->GetBrowsingContext()));
  }
  return nullptr;
}

Nullable<WindowProxyHolder> CanonicalBrowsingContext::GetTopChromeWindow() {
  RefPtr<CanonicalBrowsingContext> bc(this);
  while (RefPtr<CanonicalBrowsingContext> parent =
             bc->GetParentCrossChromeBoundary()) {
    bc = parent.forget();
  }
  if (bc->IsChrome()) {
    return WindowProxyHolder(bc.forget());
  }
  return nullptr;
}

nsISHistory* CanonicalBrowsingContext::GetSessionHistory() {
  if (!IsTop()) {
    return Cast(Top())->GetSessionHistory();
  }

  // Check GetChildSessionHistory() to make sure that this BrowsingContext has
  // session history enabled.
  if (!mSessionHistory && GetChildSessionHistory()) {
    mSessionHistory = new nsSHistory(this);
  }

  return mSessionHistory;
}

SessionHistoryEntry* CanonicalBrowsingContext::GetActiveSessionHistoryEntry() {
  return mActiveEntry;
}

UniquePtr<LoadingSessionHistoryInfo>
CanonicalBrowsingContext::CreateLoadingSessionHistoryEntryForLoad(
    nsDocShellLoadState* aLoadState, nsIChannel* aChannel) {
  RefPtr<SessionHistoryEntry> entry;
  const LoadingSessionHistoryInfo* existingLoadingInfo =
      aLoadState->GetLoadingSessionHistoryInfo();
  if (existingLoadingInfo) {
    entry = SessionHistoryEntry::GetByLoadId(existingLoadingInfo->mLoadId);
  } else {
    entry = new SessionHistoryEntry(aLoadState, aChannel);
  }
  MOZ_DIAGNOSTIC_ASSERT(entry);

  UniquePtr<LoadingSessionHistoryInfo> loadingInfo;
  if (existingLoadingInfo) {
    loadingInfo = MakeUnique<LoadingSessionHistoryInfo>(*existingLoadingInfo);
  } else {
    loadingInfo = MakeUnique<LoadingSessionHistoryInfo>(entry);
  }

  MOZ_ASSERT(SessionHistoryEntry::GetByLoadId(loadingInfo->mLoadId) == entry);
  mLoadingEntries.AppendElement(
      LoadingSessionHistoryEntry{loadingInfo->mLoadId, entry});
  return loadingInfo;
}

void CanonicalBrowsingContext::SessionHistoryCommit(uint64_t aLoadId,
                                                    const nsID& aChangeID) {
  for (size_t i = 0; i < mLoadingEntries.Length(); ++i) {
    if (mLoadingEntries[i].mLoadId == aLoadId) {
      nsISHistory* shistory = GetSessionHistory();
      if (!shistory) {
        SessionHistoryEntry::RemoveLoadId(aLoadId);
        mLoadingEntries.RemoveElementAt(i);
        return;
      }

      RefPtr<SessionHistoryEntry> oldActiveEntry = mActiveEntry.forget();
      mActiveEntry = mLoadingEntries[i].mEntry;

      SessionHistoryEntry::RemoveLoadId(aLoadId);
      mLoadingEntries.RemoveElementAt(i);
      if (IsTop()) {
        nsCOMPtr<nsISHistory> existingSHistory = mActiveEntry->GetShistory();
        if (existingSHistory) {
          MOZ_ASSERT(existingSHistory == shistory);
          shistory->UpdateIndex();
        } else {
          shistory->AddEntry(mActiveEntry,
                             /* FIXME aPersist = */ true);
        }
      } else {
        // FIXME Check if we're replacing before adding a child.
        // FIXME The old implementations adds it to the parent's mLSHE if there
        //       is one, need to figure out if that makes sense here (peterv
        //       doesn't think it would).
        // FIXME if the loading entry is already in the session history, we
        // shouldn't add a new entry.
        if (oldActiveEntry) {
          // FIXME Need to figure out the right value for aCloneChildren.
          shistory->AddChildSHEntryHelper(oldActiveEntry, mActiveEntry, Top(),
                                          true);
        } else {
          SessionHistoryEntry* parentEntry =
              static_cast<CanonicalBrowsingContext*>(GetParent())->mActiveEntry;
          if (parentEntry) {
            // FIXME The docshell code sometime uses -1 for aChildOffset!
            // FIXME Using IsInProcess for aUseRemoteSubframes isn't quite
            //       right, but aUseRemoteSubframes should be going away.
            parentEntry->AddChild(mActiveEntry, Children().Length() - 1,
                                  IsInProcess());
          }
        }
      }
      Group()->EachParent([&](ContentParent* aParent) {
        nsISHistory* shistory = GetSessionHistory();
        int32_t index = 0;
        int32_t length = 0;
        shistory->GetIndex(&index);
        shistory->GetCount(&length);
        Unused << aParent->SendHistoryCommitIndexAndLength(Top(), index, length,
                                                           aChangeID);
      });
      return;
    }
  }
  // FIXME Should we throw an error if we don't find an entry for
  // aSessionHistoryEntryId?
}

void CanonicalBrowsingContext::NotifyOnHistoryReload(
    bool& aCanReload, Maybe<RefPtr<nsDocShellLoadState>>& aLoadState,
    Maybe<bool>& aReloadActiveEntry) {
  nsISHistory* shistory = GetSessionHistory();
  shistory->NotifyOnHistoryReload(&aCanReload);
  if (!aCanReload) {
    return;
  }

  if (mActiveEntry) {
    aLoadState.emplace();
    mActiveEntry->CreateLoadInfo(getter_AddRefs(aLoadState.ref()));
    aReloadActiveEntry.emplace(true);
  } else if (!mLoadingEntries.IsEmpty()) {
    aLoadState.emplace();
    mLoadingEntries.LastElement().mEntry->CreateLoadInfo(
        getter_AddRefs(aLoadState.ref()));
    aReloadActiveEntry.emplace(false);
  }

  if (aLoadState) {
    int32_t index = 0;
    int32_t requestedIndex = -1;
    int32_t length = 0;
    shistory->GetIndex(&index);
    shistory->GetRequestedIndex(&requestedIndex);
    shistory->GetCount(&length);
    aLoadState.ref()->SetLoadIsFromSessionHistory(
        requestedIndex >= 0 ? requestedIndex : index, length);
  }
  // If we don't have an active entry and we don't have a loading entry then
  // the nsDocShell will create a load state based on its document.
}

JSObject* CanonicalBrowsingContext::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CanonicalBrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

void CanonicalBrowsingContext::DispatchWheelZoomChange(bool aIncrease) {
  Element* element = Top()->GetEmbedderElement();
  if (!element) {
    return;
  }

  auto event = aIncrease ? u"DoZoomEnlargeBy10"_ns : u"DoZoomReduceBy10"_ns;
  auto dispatcher = MakeRefPtr<AsyncEventDispatcher>(
      element, event, CanBubble::eYes, ChromeOnlyDispatch::eYes);
  dispatcher->PostDOMEvent();
}

void CanonicalBrowsingContext::CanonicalDiscard() {
  if (mTabMediaController) {
    mTabMediaController->Shutdown();
    mTabMediaController = nullptr;
  }
}

void CanonicalBrowsingContext::NotifyStartDelayedAutoplayMedia() {
  WindowContext* windowContext = GetCurrentWindowContext();
  if (!windowContext) {
    return;
  }

  // As this function would only be called when user click the play icon on the
  // tab bar. That's clear user intent to play, so gesture activate the window
  // context so that the block-autoplay logic allows the media to autoplay.
  windowContext->NotifyUserGestureActivation();
  AUTOPLAY_LOG("NotifyStartDelayedAutoplayMedia for chrome bc 0x%08" PRIx64,
               Id());
  StartDelayedAutoplayMediaComponents();
  // Notfiy all content browsing contexts which are related with the canonical
  // browsing content tree to start delayed autoplay media.

  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendStartDelayedAutoplayMediaComponents(this);
  });
}

void CanonicalBrowsingContext::NotifyMediaMutedChanged(bool aMuted,
                                                       ErrorResult& aRv) {
  MOZ_ASSERT(!GetParent(),
             "Notify media mute change on non top-level context!");
  SetMuted(aMuted, aRv);
}

uint32_t CanonicalBrowsingContext::CountSiteOrigins(
    GlobalObject& aGlobal,
    const Sequence<OwningNonNull<BrowsingContext>>& aRoots) {
  nsTHashtable<nsCStringHashKey> uniqueSiteOrigins;

  for (const auto& root : aRoots) {
    root->PreOrderWalk([&](BrowsingContext* aContext) {
      WindowGlobalParent* windowGlobalParent =
          aContext->Canonical()->GetCurrentWindowGlobal();
      if (windowGlobalParent) {
        nsIPrincipal* documentPrincipal =
            windowGlobalParent->DocumentPrincipal();

        bool isContentPrincipal = documentPrincipal->GetIsContentPrincipal();
        if (isContentPrincipal) {
          nsCString siteOrigin;
          documentPrincipal->GetSiteOrigin(siteOrigin);
          uniqueSiteOrigins.PutEntry(siteOrigin);
        }
      }
    });
  }

  return uniqueSiteOrigins.Count();
}

void CanonicalBrowsingContext::UpdateMediaControlAction(
    const MediaControlAction& aAction) {
  ContentMediaControlKeyHandler::HandleMediaControlAction(this, aAction);
  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendUpdateMediaControlAction(this, aAction);
  });
}

void CanonicalBrowsingContext::LoadURI(const nsAString& aURI,
                                       const LoadURIOptions& aOptions,
                                       ErrorResult& aError) {
  RefPtr<nsDocShellLoadState> loadState;
  nsresult rv = nsDocShellLoadState::CreateFromLoadURIOptions(
      this, aURI, aOptions, getter_AddRefs(loadState));

  if (rv == NS_ERROR_MALFORMED_URI) {
    DisplayLoadError(aURI);
    return;
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  LoadURI(loadState, true);
}

void CanonicalBrowsingContext::GoBack(
    const Optional<int32_t>& aCancelContentJSEpoch,
    bool aRequireUserInteraction) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  if (nsDocShell* docShell = nsDocShell::Cast(GetDocShell())) {
    if (aCancelContentJSEpoch.WasPassed()) {
      docShell->SetCancelContentJSEpoch(aCancelContentJSEpoch.Value());
    }
    docShell->GoBack(aRequireUserInteraction);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch = Some(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoBack(this, cancelContentJSEpoch,
                             aRequireUserInteraction);
  }
}
void CanonicalBrowsingContext::GoForward(
    const Optional<int32_t>& aCancelContentJSEpoch,
    bool aRequireUserInteraction) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  if (auto* docShell = nsDocShell::Cast(GetDocShell())) {
    if (aCancelContentJSEpoch.WasPassed()) {
      docShell->SetCancelContentJSEpoch(aCancelContentJSEpoch.Value());
    }
    docShell->GoForward(aRequireUserInteraction);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch.emplace(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoForward(this, cancelContentJSEpoch,
                                aRequireUserInteraction);
  }
}
void CanonicalBrowsingContext::GoToIndex(
    int32_t aIndex, const Optional<int32_t>& aCancelContentJSEpoch) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  if (auto* docShell = nsDocShell::Cast(GetDocShell())) {
    if (aCancelContentJSEpoch.WasPassed()) {
      docShell->SetCancelContentJSEpoch(aCancelContentJSEpoch.Value());
    }
    docShell->GotoIndex(aIndex);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch.emplace(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoToIndex(this, aIndex, cancelContentJSEpoch);
  }
}
void CanonicalBrowsingContext::Reload(uint32_t aReloadFlags) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  if (auto* docShell = nsDocShell::Cast(GetDocShell())) {
    docShell->Reload(aReloadFlags);
  } else if (ContentParent* cp = GetContentParent()) {
    Unused << cp->SendReload(this, aReloadFlags);
  }
}

void CanonicalBrowsingContext::Stop(uint32_t aStopFlags) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad && (aStopFlags & nsIWebNavigation::STOP_NETWORK)) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  // Ask the docshell to stop to handle loads that haven't
  // yet reached here, as well as non-network activity.
  if (auto* docShell = nsDocShell::Cast(GetDocShell())) {
    docShell->Stop(aStopFlags);
  } else if (ContentParent* cp = GetContentParent()) {
    Unused << cp->SendStopLoad(this, aStopFlags);
  }
}

// While process switching, we need to check if any of our ancestors are
// discarded or no longer current, in which case the process switch needs to be
// aborted.
static bool AncestorsAreCurrent(CanonicalBrowsingContext* aContext) {
  if (aContext->IsDiscarded()) {
    return false;
  }

  RefPtr<WindowGlobalParent> ancestorWindow(aContext->GetParentWindowContext());
  while (ancestorWindow) {
    // If our ancestor window is no longer the current window, or if any of our
    // ancestors have been discarded, return `false` to abort the process
    // switch.
    if (ancestorWindow->IsCached() || ancestorWindow->IsDiscarded() ||
        ancestorWindow->GetBrowsingContext()->IsDiscarded()) {
      return false;
    }
    ancestorWindow = ancestorWindow->GetParentWindowContext();
  }
  return true;
}

void CanonicalBrowsingContext::PendingRemotenessChange::ProcessReady() {
  if (!mPromise) {
    return;
  }

  // Wait for our blocker promise to resolve, if present.
  if (mPrepareToChangePromise) {
    mPrepareToChangePromise->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self = RefPtr{this}](bool) { self->Finish(); },
        [self = RefPtr{this}](nsresult aRv) { self->Cancel(aRv); });
    return;
  }

  Finish();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Finish() {
  if (!mPromise) {
    return;
  }

  RefPtr<CanonicalBrowsingContext> target(mTarget);
  if (target->IsDiscarded()) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  if (!AncestorsAreCurrent(target)) {
    NS_WARNING("Ancestor context is no longer current");
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  // If this BrowsingContext is embedded within the parent process, perform the
  // process switch directly.
  if (Element* browserElement = target->GetEmbedderElement()) {
    MOZ_DIAGNOSTIC_ASSERT(target->IsTop(),
                          "We shouldn't be trying to change the remoteness of "
                          "non-remote iframes");

    nsCOMPtr<nsIBrowser> browser = browserElement->AsBrowser();
    if (!browser) {
      Cancel(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
        do_QueryObject(browserElement);
    MOZ_RELEASE_ASSERT(frameLoaderOwner,
                       "embedder browser must be nsFrameLoaderOwner");

    // Tell frontend code that this browser element is about to change process.
    nsresult rv = browser->BeforeChangeRemoteness();
    if (NS_FAILED(rv)) {
      Cancel(rv);
      return;
    }

    // Some frontend code checks the value of the `remote` attribute on the
    // browser to determine if it is remote, so update the value.
    browserElement->SetAttr(kNameSpaceID_None, nsGkAtoms::remote,
                            mContentParent ? u"true"_ns : u"false"_ns,
                            /* notify */ true);

    // The process has been created, hand off to nsFrameLoaderOwner to finish
    // the process switch.
    ErrorResult error;
    frameLoaderOwner->ChangeRemotenessToProcess(
        mContentParent, mReplaceBrowsingContext, mSpecificGroup, error);
    if (error.Failed()) {
      Cancel(error.StealNSResult());
      return;
    }

    // Tell frontend the load is done.
    bool loadResumed = false;
    rv = browser->FinishChangeRemoteness(mPendingSwitchId, &loadResumed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      Cancel(rv);
      return;
    }

    // We did it! The process switch is complete.
    RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    RefPtr<BrowserParent> newBrowser = frameLoader->GetBrowserParent();
    if (!newBrowser) {
      if (mContentParent) {
        // Failed to create the BrowserParent somehow! Abort the process switch
        // attempt.
        Cancel(NS_ERROR_UNEXPECTED);
        return;
      }

      if (!loadResumed) {
        RefPtr<nsDocShell> newDocShell = frameLoader->GetDocShell(error);
        if (error.Failed()) {
          Cancel(error.StealNSResult());
          return;
        }

        rv = newDocShell->ResumeRedirectedLoad(mPendingSwitchId,
                                               /* aHistoryIndex */ -1);
        if (NS_FAILED(rv)) {
          Cancel(error.StealNSResult());
          return;
        }
      }
    } else if (!loadResumed) {
      newBrowser->ResumeLoad(mPendingSwitchId);
    }

    mPromise->Resolve(newBrowser, __func__);
    Clear();
    return;
  }

  if (NS_WARN_IF(!mContentParent)) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<WindowGlobalParent> embedderWindow = target->GetEmbedderWindowGlobal();
  if (NS_WARN_IF(!embedderWindow) || NS_WARN_IF(!embedderWindow->CanSend())) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<BrowserParent> embedderBrowser = embedderWindow->GetBrowserParent();
  if (NS_WARN_IF(!embedderBrowser)) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  // Pull load flags from our embedder browser.
  nsCOMPtr<nsILoadContext> loadContext = embedderBrowser->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(
      loadContext->UseRemoteTabs() && loadContext->UseRemoteSubframes(),
      "Not supported without fission");

  // NOTE: These are the only flags we actually care about
  uint32_t chromeFlags = nsIWebBrowserChrome::CHROME_REMOTE_WINDOW |
                         nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  if (loadContext->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }

  RefPtr<WindowGlobalParent> oldWindow = target->GetCurrentWindowGlobal();
  RefPtr<BrowserParent> oldBrowser =
      oldWindow ? oldWindow->GetBrowserParent() : nullptr;
  bool wasRemote = oldWindow && oldWindow->IsProcessRoot();

  // Update which process is considered the current owner
  uint64_t inFlightProcessId = target->OwnerProcessId();
  target->SetInFlightProcessId(inFlightProcessId);
  target->SetOwnerProcessId(mContentParent->ChildID());

  auto resetInFlightId = [target, inFlightProcessId] {
    target->ClearInFlightProcessId(inFlightProcessId);
  };

  // If we were in a remote frame, trigger unloading of the remote window. When
  // the original remote window acknowledges, we can clear the in-flight ID.
  if (wasRemote) {
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser != embedderBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser->GetBrowserBridgeParent());

    auto callback = [resetInFlightId](auto) { resetInFlightId(); };
    oldBrowser->SendWillChangeProcess(callback, callback);
    oldBrowser->Destroy();
  }

  MOZ_ASSERT(!mReplaceBrowsingContext, "Cannot replace BC for subframe");
  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(
          target->OriginAttributesRef(),
          /* isFirstParty */ false);
  WindowGlobalInit windowInit =
      WindowGlobalActor::AboutBlankInitializer(target, initialPrincipal);

  // Create and initialize our new BrowserBridgeParent.
  TabId tabId(nsContentUtils::GenerateTabId());
  RefPtr<BrowserBridgeParent> bridge = new BrowserBridgeParent();
  nsresult rv = bridge->InitWithProcess(embedderBrowser, mContentParent,
                                        windowInit, chromeFlags, tabId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return;
  }

  // Tell the embedder process a remoteness change is in-process. When this is
  // acknowledged, reset the in-flight ID if it used to be an in-process load.
  RefPtr<BrowserParent> newBrowser = bridge->GetBrowserParent();
  {
    auto callback = [wasRemote, resetInFlightId](auto) {
      if (!wasRemote) {
        resetInFlightId();
      }
    };

    ManagedEndpoint<PBrowserBridgeChild> endpoint =
        embedderBrowser->OpenPBrowserBridgeEndpoint(bridge);
    if (NS_WARN_IF(!endpoint.IsValid())) {
      Cancel(NS_ERROR_UNEXPECTED);
      return;
    }
    embedderWindow->SendMakeFrameRemote(target, std::move(endpoint), tabId,
                                        newBrowser->GetLayersId(), callback,
                                        callback);
  }

  // Resume the pending load in our new process.
  newBrowser->ResumeLoad(mPendingSwitchId);

  // We did it! The process switch is complete.
  mPromise->Resolve(newBrowser, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Cancel(nsresult aRv) {
  if (!mPromise) {
    return;
  }

  mPromise->Reject(aRv, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Clear() {
  // Make sure we don't die while we're doing cleanup.
  RefPtr<PendingRemotenessChange> kungFuDeathGrip(this);
  if (mTarget) {
    MOZ_DIAGNOSTIC_ASSERT(mTarget->mPendingRemotenessChange == this);
    mTarget->mPendingRemotenessChange = nullptr;
  }

  // When this PendingRemotenessChange was created, it was given a
  // `mContentParent`.
  if (mContentParent) {
    mContentParent->RemoveKeepAlive();
    mContentParent = nullptr;
  }

  // If we were given a specific group, stop keeping that group alive manually.
  if (mSpecificGroup) {
    mSpecificGroup->RemoveKeepAlive();
    mSpecificGroup = nullptr;
  }

  mPromise = nullptr;
  mTarget = nullptr;
  mPrepareToChangePromise = nullptr;
}

CanonicalBrowsingContext::PendingRemotenessChange::PendingRemotenessChange(
    CanonicalBrowsingContext* aTarget, RemotenessPromise::Private* aPromise,
    uint64_t aPendingSwitchId, bool aReplaceBrowsingContext)
    : mTarget(aTarget),
      mPromise(aPromise),
      mPendingSwitchId(aPendingSwitchId),
      mReplaceBrowsingContext(aReplaceBrowsingContext) {}

CanonicalBrowsingContext::PendingRemotenessChange::~PendingRemotenessChange() {
  MOZ_ASSERT(!mPromise && !mTarget && !mContentParent && !mSpecificGroup &&
                 !mPrepareToChangePromise,
             "should've already been Cancel() or Complete()-ed");
}

RefPtr<CanonicalBrowsingContext::RemotenessPromise>
CanonicalBrowsingContext::ChangeRemoteness(const nsACString& aRemoteType,
                                           uint64_t aPendingSwitchId,
                                           bool aReplaceBrowsingContext,
                                           uint64_t aSpecificGroupId) {
  MOZ_DIAGNOSTIC_ASSERT(IsContent(),
                        "cannot change the process of chrome contexts");
  MOZ_DIAGNOSTIC_ASSERT(
      IsTop() == IsEmbeddedInProcess(0),
      "toplevel content must be embedded in the parent process");
  MOZ_DIAGNOSTIC_ASSERT(!aReplaceBrowsingContext || IsTop(),
                        "Cannot replace BrowsingContext for subframes");
  MOZ_DIAGNOSTIC_ASSERT(aSpecificGroupId == 0 || aReplaceBrowsingContext,
                        "Cannot specify group ID unless replacing BC");

  if (!AncestorsAreCurrent(this)) {
    NS_WARNING("An ancestor context is no longer current");
    return RemotenessPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Ensure our embedder hasn't been destroyed already.
  RefPtr<WindowGlobalParent> embedderWindowGlobal = GetEmbedderWindowGlobal();
  if (!embedderWindowGlobal) {
    NS_WARNING("Non-embedded BrowsingContext");
    return RemotenessPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  if (!embedderWindowGlobal->CanSend()) {
    NS_WARNING("Embedder already been destroyed.");
    return RemotenessPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  if (aRemoteType.IsEmpty() && (!IsTop() || !GetEmbedderElement())) {
    NS_WARNING("Cannot load non-remote subframes");
    return RemotenessPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Cancel ongoing remoteness changes.
  if (mPendingRemotenessChange) {
    mPendingRemotenessChange->Cancel(NS_ERROR_ABORT);
    MOZ_DIAGNOSTIC_ASSERT(!mPendingRemotenessChange, "Should have cleared");
  }

  RefPtr<BrowserParent> embedderBrowser =
      embedderWindowGlobal->GetBrowserParent();
  // Switching to local. No new process, so perform switch sync.
  if (embedderBrowser &&
      aRemoteType.Equals(embedderBrowser->Manager()->GetRemoteType())) {
    if (GetCurrentWindowGlobal()) {
      MOZ_DIAGNOSTIC_ASSERT(GetCurrentWindowGlobal()->IsProcessRoot());
      RefPtr<BrowserParent> oldBrowser =
          GetCurrentWindowGlobal()->GetBrowserParent();

      uint64_t targetProcessId = OwnerProcessId();
      SetInFlightProcessId(targetProcessId);
      auto callback = [target = RefPtr{this}, targetProcessId](auto) {
        target->ClearInFlightProcessId(targetProcessId);
      };
      oldBrowser->SendWillChangeProcess(callback, callback);
      oldBrowser->Destroy();
    }

    // If the embedder process is remote, tell that remote process to become
    // the owner.
    MOZ_DIAGNOSTIC_ASSERT(!aReplaceBrowsingContext);
    MOZ_DIAGNOSTIC_ASSERT(!aRemoteType.IsEmpty());
    SetOwnerProcessId(embedderBrowser->Manager()->ChildID());
    Unused << embedderWindowGlobal->SendMakeFrameLocal(this, aPendingSwitchId);
    return RemotenessPromise::CreateAndResolve(embedderBrowser, __func__);
  }

  // Switching to remote. Wait for new process to launch before switch.
  auto promise = MakeRefPtr<RemotenessPromise::Private>(__func__);
  RefPtr<PendingRemotenessChange> change = new PendingRemotenessChange(
      this, promise, aPendingSwitchId, aReplaceBrowsingContext);
  mPendingRemotenessChange = change;

  // If a specific BrowsingContextGroup ID was specified for this load, make
  // sure to keep it alive until the process switch is completed.
  if (aSpecificGroupId) {
    change->mSpecificGroup =
        BrowsingContextGroup::GetOrCreate(aSpecificGroupId);
    change->mSpecificGroup->AddKeepAlive();
  }

  // Call `prepareToChangeRemoteness` in parallel with starting a new process
  // for <browser> loads.
  if (IsTop() && GetEmbedderElement()) {
    nsCOMPtr<nsIBrowser> browser = GetEmbedderElement()->AsBrowser();
    if (!browser) {
      change->Cancel(NS_ERROR_FAILURE);
      return promise.forget();
    }

    RefPtr<Promise> blocker;
    nsresult rv = browser->PrepareToChangeRemoteness(getter_AddRefs(blocker));
    if (NS_FAILED(rv)) {
      change->Cancel(rv);
      return promise.forget();
    }
    change->mPrepareToChangePromise = GenericPromise::FromDomPromise(blocker);
  }

  if (aRemoteType.IsEmpty()) {
    change->ProcessReady();
  } else {
    // Try to predict which BrowsingContextGroup will be used for the final load
    // in this BrowsingContext. This has to be accurate if switching into an
    // existing group, as it will control what pool of processes will be used
    // for process selection.
    //
    // It's _technically_ OK to provide a group here if we're actually going to
    // switch into a brand new group, though it's sub-optimal, as it can
    // restrict the set of processes we're using.
    BrowsingContextGroup* finalGroup =
        aReplaceBrowsingContext ? change->mSpecificGroup.get() : Group();

    change->mContentParent = ContentParent::GetNewOrUsedLaunchingBrowserProcess(
        /* aRemoteType = */ aRemoteType,
        /* aGroup = */ finalGroup,
        /* aPriority = */ hal::PROCESS_PRIORITY_FOREGROUND,
        /* aPreferUsed = */ false);
    if (!change->mContentParent) {
      change->Cancel(NS_ERROR_FAILURE);
      return promise.forget();
    }

    // Add a KeepAlive used by this ContentParent, which will be cleared when
    // the change is complete. This should prevent the process dying before
    // we're ready to use it.
    change->mContentParent->AddKeepAlive();
    change->mContentParent->WaitForLaunchAsync()->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [change](ContentParent*) { change->ProcessReady(); },
        [change](LaunchError) { change->Cancel(NS_ERROR_FAILURE); });
  }
  return promise.forget();
}

MediaController* CanonicalBrowsingContext::GetMediaController() {
  // We would only create one media controller per tab, so accessing the
  // controller via the top-level browsing context.
  if (GetParent()) {
    return Cast(Top())->GetMediaController();
  }

  MOZ_ASSERT(!GetParent(),
             "Must access the controller from the top-level browsing context!");
  // Only content browsing context can create media controller, we won't create
  // controller for chrome document, such as the browser UI.
  if (!mTabMediaController && !IsDiscarded() && IsContent()) {
    mTabMediaController = new MediaController(Id());
  }
  return mTabMediaController;
}

bool CanonicalBrowsingContext::SupportsLoadingInParent(
    nsDocShellLoadState* aLoadState, uint64_t* aOuterWindowId) {
  // We currently don't support initiating loads in the parent when they are
  // watched by devtools. This is because devtools tracks loads using content
  // process notifications, which happens after the load is initiated in this
  // case. Devtools clears all prior requests when it detects a new navigation,
  // so it drops the main document load that happened here.
  if (WatchedByDevTools()) {
    return false;
  }

  // DocumentChannel currently only supports connecting channels into the
  // content process, so we can only support schemes that will always be loaded
  // there for now. Restrict to just http(s) for simplicity.
  if (!net::SchemeIsHTTP(aLoadState->URI()) &&
      !net::SchemeIsHTTPS(aLoadState->URI())) {
    return false;
  }

  if (WindowGlobalParent* global = GetCurrentWindowGlobal()) {
    nsCOMPtr<nsIURI> currentURI = global->GetDocumentURI();
    if (currentURI) {
      bool newURIHasRef = false;
      aLoadState->URI()->GetHasRef(&newURIHasRef);
      bool equalsExceptRef = false;
      aLoadState->URI()->EqualsExceptRef(currentURI, &equalsExceptRef);

      if (equalsExceptRef && newURIHasRef) {
        // This navigation is same-doc WRT the current one, we should pass it
        // down to the docshell to be handled.
        return false;
      }
    }
    // If the current document has a beforeunload listener, then we need to
    // start the load in that process after we fire the event.
    if (global->HasBeforeUnload()) {
      return false;
    }

    *aOuterWindowId = global->OuterWindowId();
  }
  return true;
}

bool CanonicalBrowsingContext::LoadInParent(nsDocShellLoadState* aLoadState,
                                            bool aSetNavigating) {
  // We currently only support starting loads directly from the
  // CanonicalBrowsingContext for top-level BCs.
  // We currently only support starting loads directly from the
  // CanonicalBrowsingContext for top-level BCs.
  if (!IsTopContent() || !GetContentParent() ||
      !StaticPrefs::browser_tabs_documentchannel_parent_controlled()) {
    return false;
  }

  uint64_t outerWindowId = 0;
  if (!SupportsLoadingInParent(aLoadState, &outerWindowId)) {
    return false;
  }

  // Note: If successful, this will recurse into StartDocumentLoad and
  // set mCurrentLoad to the DocumentLoadListener instance created.
  // Ideally in the future we will only start loads from here, and we can
  // just set this directly instead.
  return net::DocumentLoadListener::LoadInParent(this, aLoadState,
                                                 aSetNavigating);
}

bool CanonicalBrowsingContext::AttemptSpeculativeLoadInParent(
    nsDocShellLoadState* aLoadState) {
  // We currently only support starting loads directly from the
  // CanonicalBrowsingContext for top-level BCs.
  // We currently only support starting loads directly from the
  // CanonicalBrowsingContext for top-level BCs.
  if (!IsTopContent() || !GetContentParent() ||
      StaticPrefs::browser_tabs_documentchannel_parent_controlled()) {
    return false;
  }

  uint64_t outerWindowId = 0;
  if (!SupportsLoadingInParent(aLoadState, &outerWindowId)) {
    return false;
  }

  // If we successfully open the DocumentChannel, then it'll register
  // itself using aLoadIdentifier and be kept alive until it completes
  // loading.
  return net::DocumentLoadListener::SpeculativeLoadInParent(this, aLoadState);
}

bool CanonicalBrowsingContext::StartDocumentLoad(
    net::DocumentLoadListener* aLoad) {
  // If we're controlling loads from the parent, then starting a new load means
  // that we need to cancel any existing ones.
  if (StaticPrefs::browser_tabs_documentchannel_parent_controlled() &&
      mCurrentLoad) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }
  mCurrentLoad = aLoad;

  if (NS_FAILED(SetCurrentLoadIdentifier(Some(aLoad->GetLoadIdentifier())))) {
    mCurrentLoad = nullptr;
    return false;
  }

  return true;
}

void CanonicalBrowsingContext::EndDocumentLoad(bool aForProcessSwitch) {
  mCurrentLoad = nullptr;

  if (!aForProcessSwitch) {
    // Resetting the current load identifier on a discarded context
    // has no effect when a document load has finished.
    Unused << SetCurrentLoadIdentifier(Nothing());
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanonicalBrowsingContext, BrowsingContext,
                                   mSessionHistory)

NS_IMPL_ADDREF_INHERITED(CanonicalBrowsingContext, BrowsingContext)
NS_IMPL_RELEASE_INHERITED(CanonicalBrowsingContext, BrowsingContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanonicalBrowsingContext)
NS_INTERFACE_MAP_END_INHERITING(BrowsingContext)

}  // namespace dom
}  // namespace mozilla

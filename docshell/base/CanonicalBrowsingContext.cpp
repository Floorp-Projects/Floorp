/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CanonicalBrowsingContext.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/PWindowGlobalParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/MediaController.h"
#include "mozilla/dom/MediaControlService.h"
#include "mozilla/dom/ContentPlaybackController.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPrefs_docshell.h"
#include "mozilla/StaticPrefs_fission.h"
#include "nsIWebNavigation.h"
#include "mozilla/MozPromiseInlines.h"
#include "nsDocShell.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowOuter.h"
#include "nsIWebBrowserChrome.h"
#include "nsIXULRuntime.h"
#include "nsNetUtil.h"
#include "nsSHistory.h"
#include "nsSecureBrowserUI.h"
#include "nsQueryObject.h"
#include "nsBrowserStatusFilter.h"
#include "nsIBrowser.h"
#include "nsTHashSet.h"

#ifdef NS_PRINTING
#  include "mozilla/embedding/printingui/PrintingParent.h"
#  include "nsIWebBrowserPrint.h"
#endif

using namespace mozilla::ipc;

extern mozilla::LazyLogModule gAutoplayPermissionLog;
extern mozilla::LazyLogModule gSHLog;
extern mozilla::LazyLogModule gSHIPBFCacheLog;

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

  // The initial URI in a BrowsingContext is always "about:blank".
  MOZ_ALWAYS_SUCCEEDS(
      NS_NewURI(getter_AddRefs(mCurrentRemoteURI), "about:blank"));
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
    CanonicalBrowsingContext* aNewContext,
    const RemotenessChangeOptions& aRemotenessOptions) {
  MOZ_ASSERT(!aNewContext->mWebProgress);
  MOZ_ASSERT(!aNewContext->mSessionHistory);
  MOZ_ASSERT(IsTop() && aNewContext->IsTop());
  if (mStatusFilter) {
    mStatusFilter->RemoveProgressListener(mWebProgress);
    mStatusFilter = nullptr;
  }
  aNewContext->mWebProgress = std::move(mWebProgress);

  // Use the Transaction for the fields which need to be updated whether or not
  // the new context has been attached before.
  // SetWithoutSyncing can be used if context hasn't been attached.
  Transaction txn;
  txn.SetBrowserId(GetBrowserId());
  txn.SetHistoryID(GetHistoryID());
  txn.SetExplicitActive(GetExplicitActive());
  txn.SetHasRestoreData(GetHasRestoreData());
  if (aNewContext->EverAttached()) {
    MOZ_ALWAYS_SUCCEEDS(txn.Commit(aNewContext));
  } else {
    txn.CommitWithoutSyncing(aNewContext);
  }

  aNewContext->mRestoreState = mRestoreState.forget();
  MOZ_ALWAYS_SUCCEEDS(SetHasRestoreData(false));

  // XXXBFCache name handling is still a bit broken in Fission in general,
  // at least in case name should be cleared.
  if (aRemotenessOptions.mTryUseBFCache) {
    MOZ_ASSERT(!aNewContext->EverAttached());
    aNewContext->mFields.SetWithoutSyncing<IDX_Name>(GetName());
    aNewContext->mFields.SetWithoutSyncing<IDX_HasLoadedNonInitialDocument>(
        GetHasLoadedNonInitialDocument());
  }

  if (mSessionHistory) {
    mSessionHistory->SetBrowsingContext(aNewContext);
    if (mozilla::BFCacheInParent()) {
      // XXXBFCache Should we clear the epoch always?
      mSessionHistory->SetEpoch(0, Nothing());
    }
    mSessionHistory.swap(aNewContext->mSessionHistory);
    RefPtr<ChildSHistory> childSHistory = ForgetChildSHistory();
    aNewContext->SetChildSHistory(childSHistory);
  }

  if (mozilla::SessionHistoryInParent()) {
    BackgroundSessionStorageManager::PropagateManager(Id(), aNewContext->Id());
  }

  // Transfer the ownership of the priority active status from the old context
  // to the new context.
  aNewContext->mPriorityActive = mPriorityActive;
  mPriorityActive = false;

  MOZ_ASSERT(aNewContext->mLoadingEntries.IsEmpty());
  mLoadingEntries.SwapElements(aNewContext->mLoadingEntries);
  MOZ_ASSERT(!aNewContext->mActiveEntry);
  mActiveEntry.swap(aNewContext->mActiveEntry);
}

void CanonicalBrowsingContext::UpdateSecurityState() {
  if (mSecureBrowserUI) {
    mSecureBrowserUI->RecomputeSecurityFlags();
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

already_AddRefed<CanonicalBrowsingContext>
CanonicalBrowsingContext::TopCrossChromeBoundary() {
  RefPtr<CanonicalBrowsingContext> bc(this);
  while (RefPtr<CanonicalBrowsingContext> parent =
             bc->GetParentCrossChromeBoundary()) {
    bc = parent.forget();
  }
  return bc.forget();
}

Nullable<WindowProxyHolder> CanonicalBrowsingContext::GetTopChromeWindow() {
  RefPtr<CanonicalBrowsingContext> bc = TopCrossChromeBoundary();
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

void CanonicalBrowsingContext::SetActiveSessionHistoryEntry(
    SessionHistoryEntry* aEntry) {
  mActiveEntry = aEntry;
}

bool CanonicalBrowsingContext::HasHistoryEntry(nsISHEntry* aEntry) {
  // XXX Should we check also loading entries?
  return aEntry && mActiveEntry == aEntry;
}

void CanonicalBrowsingContext::SwapHistoryEntries(nsISHEntry* aOldEntry,
                                                  nsISHEntry* aNewEntry) {
  // XXX Should we check also loading entries?
  if (mActiveEntry == aOldEntry) {
    nsCOMPtr<SessionHistoryEntry> newEntry = do_QueryInterface(aNewEntry);
    mActiveEntry = newEntry.forget();
  }
}

void CanonicalBrowsingContext::AddLoadingSessionHistoryEntry(
    uint64_t aLoadId, SessionHistoryEntry* aEntry) {
  Unused << SetHistoryID(aEntry->DocshellID());
  mLoadingEntries.AppendElement(LoadingSessionHistoryEntry{aLoadId, aEntry});
}

void CanonicalBrowsingContext::GetLoadingSessionHistoryInfoFromParent(
    Maybe<LoadingSessionHistoryInfo>& aLoadingInfo, int32_t* aRequestedIndex,
    int32_t* aLength) {
  *aRequestedIndex = -1;
  *aLength = 0;

  nsISHistory* shistory = GetSessionHistory();
  if (!shistory || !GetParent()) {
    return;
  }

  SessionHistoryEntry* parentSHE =
      GetParent()->Canonical()->GetActiveSessionHistoryEntry();
  if (parentSHE) {
    int32_t index = -1;
    for (BrowsingContext* sibling : GetParent()->Children()) {
      ++index;
      if (sibling == this) {
        nsCOMPtr<nsISHEntry> shEntry;
        parentSHE->GetChildSHEntryIfHasNoDynamicallyAddedChild(
            index, getter_AddRefs(shEntry));
        nsCOMPtr<SessionHistoryEntry> she = do_QueryInterface(shEntry);
        if (she) {
          aLoadingInfo.emplace(she);
          mLoadingEntries.AppendElement(LoadingSessionHistoryEntry{
              aLoadingInfo.value().mLoadId, she.get()});
          *aRequestedIndex = shistory->GetRequestedIndex();
          *aLength = shistory->GetCount();
          Unused << SetHistoryID(she->DocshellID());
        }
        break;
      }
    }
  }
}

UniquePtr<LoadingSessionHistoryInfo>
CanonicalBrowsingContext::CreateLoadingSessionHistoryEntryForLoad(
    nsDocShellLoadState* aLoadState, nsIChannel* aChannel) {
  RefPtr<SessionHistoryEntry> entry;
  const LoadingSessionHistoryInfo* existingLoadingInfo =
      aLoadState->GetLoadingSessionHistoryInfo();
  if (existingLoadingInfo) {
    entry = SessionHistoryEntry::GetByLoadId(existingLoadingInfo->mLoadId);
    MOZ_LOG(gSHLog, LogLevel::Verbose,
            ("SHEntry::GetByLoadId(%" PRIu64 ") -> %p",
             existingLoadingInfo->mLoadId, entry.get()));
    if (!entry) {
      return nullptr;
    }
    Unused << SetHistoryEntryCount(entry->BCHistoryLength());
  } else {
    entry = new SessionHistoryEntry(aLoadState, aChannel);
    if (IsTop()) {
      // Only top level pages care about Get/SetPersist.
      entry->SetPersist(
          nsDocShell::ShouldAddToSessionHistory(aLoadState->URI(), aChannel));
    } else if (mActiveEntry || !mLoadingEntries.IsEmpty()) {
      entry->SetIsSubFrame(true);
    }
    entry->SetDocshellID(GetHistoryID());
    entry->SetIsDynamicallyAdded(CreatedDynamically());
    entry->SetForInitialLoad(true);
  }
  MOZ_DIAGNOSTIC_ASSERT(entry);

  UniquePtr<LoadingSessionHistoryInfo> loadingInfo;
  if (existingLoadingInfo) {
    loadingInfo = MakeUnique<LoadingSessionHistoryInfo>(*existingLoadingInfo);
  } else {
    loadingInfo = MakeUnique<LoadingSessionHistoryInfo>(entry);
    mLoadingEntries.AppendElement(
        LoadingSessionHistoryEntry{loadingInfo->mLoadId, entry});
  }

  MOZ_ASSERT(SessionHistoryEntry::GetByLoadId(loadingInfo->mLoadId) == entry);

  return loadingInfo;
}

UniquePtr<LoadingSessionHistoryInfo>
CanonicalBrowsingContext::ReplaceLoadingSessionHistoryEntryForLoad(
    LoadingSessionHistoryInfo* aInfo, nsIChannel* aChannel) {
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(aChannel);

  UniquePtr<SessionHistoryInfo> newInfo = MakeUnique<SessionHistoryInfo>(
      aChannel, aInfo->mInfo.LoadType(),
      aInfo->mInfo.GetPartitionedPrincipalToInherit(), aInfo->mInfo.GetCsp());

  RefPtr<SessionHistoryEntry> newEntry = new SessionHistoryEntry(newInfo.get());
  if (IsTop()) {
    // Only top level pages care about Get/SetPersist.
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    newEntry->SetPersist(nsDocShell::ShouldAddToSessionHistory(uri, aChannel));
  } else {
    newEntry->SetIsSubFrame(aInfo->mInfo.IsSubFrame());
  }
  newEntry->SetDocshellID(GetHistoryID());
  newEntry->SetIsDynamicallyAdded(CreatedDynamically());

  // Replacing the old entry.
  SessionHistoryEntry::SetByLoadId(aInfo->mLoadId, newEntry);

  bool forInitialLoad = true;
  for (size_t i = 0; i < mLoadingEntries.Length(); ++i) {
    if (mLoadingEntries[i].mLoadId == aInfo->mLoadId) {
      forInitialLoad = mLoadingEntries[i].mEntry->ForInitialLoad();
      mLoadingEntries[i].mEntry = newEntry;
      break;
    }
  }

  newEntry->SetForInitialLoad(forInitialLoad);

  return MakeUnique<LoadingSessionHistoryInfo>(newEntry, aInfo->mLoadId);
}

#ifdef NS_PRINTING
class PrintListenerAdapter final : public nsIWebProgressListener {
 public:
  explicit PrintListenerAdapter(Promise* aPromise) : mPromise(aPromise) {}

  NS_DECL_ISUPPORTS

  // NS_DECL_NSIWEBPROGRESSLISTENER
  NS_IMETHOD OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                           uint32_t aStateFlags, nsresult aStatus) override {
    if (aStateFlags & nsIWebProgressListener::STATE_STOP &&
        aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT && mPromise) {
      mPromise->MaybeResolveWithUndefined();
      mPromise = nullptr;
    }
    return NS_OK;
  }
  NS_IMETHOD OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                            nsresult aStatus,
                            const char16_t* aMessage) override {
    if (aStatus != NS_OK && mPromise) {
      mPromise->MaybeReject(ErrorResult(aStatus));
      mPromise = nullptr;
    }
    return NS_OK;
  }
  NS_IMETHOD OnProgressChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest, int32_t aCurSelfProgress,
                              int32_t aMaxSelfProgress,
                              int32_t aCurTotalProgress,
                              int32_t aMaxTotalProgress) override {
    return NS_OK;
  }
  NS_IMETHOD OnLocationChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest, nsIURI* aLocation,
                              uint32_t aFlags) override {
    return NS_OK;
  }
  NS_IMETHOD OnSecurityChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest, uint32_t aState) override {
    return NS_OK;
  }
  NS_IMETHOD OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    uint32_t aEvent) override {
    return NS_OK;
  }

 private:
  ~PrintListenerAdapter() = default;

  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(PrintListenerAdapter, nsIWebProgressListener)
#endif

already_AddRefed<Promise> CanonicalBrowsingContext::Print(
    nsIPrintSettings* aPrintSettings, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(GetIncumbentGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return promise.forget();
  }

#ifndef NS_PRINTING
  promise->MaybeReject(ErrorResult(NS_ERROR_NOT_AVAILABLE));
  return promise.forget();
#else

  auto listener = MakeRefPtr<PrintListenerAdapter>(promise);
  if (IsInProcess()) {
    RefPtr<nsGlobalWindowOuter> outerWindow =
        nsGlobalWindowOuter::Cast(GetDOMWindow());
    if (NS_WARN_IF(!outerWindow)) {
      promise->MaybeReject(ErrorResult(NS_ERROR_FAILURE));
      return promise.forget();
    }

    ErrorResult rv;
    outerWindow->Print(aPrintSettings, listener,
                       /* aDocShellToCloneInto = */ nullptr,
                       nsGlobalWindowOuter::IsPreview::No,
                       nsGlobalWindowOuter::IsForWindowDotPrint::No,
                       /* aPrintPreviewCallback = */ nullptr, rv);
    if (rv.Failed()) {
      promise->MaybeReject(std::move(rv));
    }
    return promise.forget();
  }

  auto* browserParent = GetBrowserParent();
  if (NS_WARN_IF(!browserParent)) {
    promise->MaybeReject(ErrorResult(NS_ERROR_FAILURE));
    return promise.forget();
  }

  RefPtr<embedding::PrintingParent> printingParent =
      browserParent->Manager()->GetPrintingParent();

  embedding::PrintData printData;
  nsresult rv = printingParent->SerializeAndEnsureRemotePrintJob(
      aPrintSettings, listener, nullptr, &printData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(ErrorResult(rv));
    return promise.forget();
  }

  if (NS_WARN_IF(!browserParent->SendPrint(this, printData))) {
    promise->MaybeReject(ErrorResult(NS_ERROR_FAILURE));
  }
  return promise.forget();
#endif
}

void CanonicalBrowsingContext::CallOnAllTopDescendants(
    const std::function<mozilla::CallState(CanonicalBrowsingContext*)>&
        aCallback) {
#ifdef DEBUG
  RefPtr<CanonicalBrowsingContext> parent = GetParentCrossChromeBoundary();
  MOZ_ASSERT(!parent, "Should only call on top chrome BC");
#endif

  nsTArray<RefPtr<BrowsingContextGroup>> groups;
  BrowsingContextGroup::GetAllGroups(groups);
  for (auto& browsingContextGroup : groups) {
    for (auto& bc : browsingContextGroup->Toplevels()) {
      if (bc == this) {
        // Cannot be a descendent of myself so skip.
        continue;
      }

      RefPtr<CanonicalBrowsingContext> top =
          bc->Canonical()->TopCrossChromeBoundary();
      if (top == this) {
        if (aCallback(bc->Canonical()) == CallState::Stop) {
          return;
        }
      }
    }
  }
}

void CanonicalBrowsingContext::SessionHistoryCommit(uint64_t aLoadId,
                                                    const nsID& aChangeID,
                                                    uint32_t aLoadType,
                                                    bool aPersist,
                                                    bool aCloneEntryChildren) {
  MOZ_LOG(gSHLog, LogLevel::Verbose,
          ("CanonicalBrowsingContext::SessionHistoryCommit %p %" PRIu64, this,
           aLoadId));
  for (size_t i = 0; i < mLoadingEntries.Length(); ++i) {
    if (mLoadingEntries[i].mLoadId == aLoadId) {
      nsSHistory* shistory = static_cast<nsSHistory*>(GetSessionHistory());
      if (!shistory) {
        SessionHistoryEntry::RemoveLoadId(aLoadId);
        mLoadingEntries.RemoveElementAt(i);
        return;
      }

      CallerWillNotifyHistoryIndexAndLengthChanges caller(shistory);

      RefPtr<SessionHistoryEntry> newActiveEntry = mLoadingEntries[i].mEntry;

      bool loadFromSessionHistory = !newActiveEntry->ForInitialLoad();
      newActiveEntry->SetForInitialLoad(false);
      SessionHistoryEntry::RemoveLoadId(aLoadId);
      mLoadingEntries.RemoveElementAt(i);

      // If there is a name in the new entry, clear the name of all contiguous
      // entries. This is for https://html.spec.whatwg.org/#history-traversal
      // Step 4.4.2.
      nsAutoString nameOfNewEntry;
      newActiveEntry->GetName(nameOfNewEntry);
      if (!nameOfNewEntry.IsEmpty()) {
        nsSHistory::WalkContiguousEntries(
            newActiveEntry,
            [](nsISHEntry* aEntry) { aEntry->SetName(EmptyString()); });
      }

      bool addEntry = ShouldUpdateSessionHistory(aLoadType);
      if (IsTop()) {
        if (mActiveEntry && !mActiveEntry->GetFrameLoader()) {
          bool sharesDocument = true;
          mActiveEntry->SharesDocumentWith(newActiveEntry, &sharesDocument);
          if (!sharesDocument) {
            // If the old page won't be in the bfcache,
            // clear the dynamic entries.
            RemoveDynEntriesFromActiveSessionHistoryEntry();
          }
        }
        mActiveEntry = newActiveEntry;

        if (LOAD_TYPE_HAS_FLAGS(aLoadType,
                                nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY)) {
          // Replace the current entry with the new entry.
          int32_t index = shistory->GetIndexForReplace();

          // If we're trying to replace an inexistant shistory entry then we
          // should append instead.
          addEntry = index < 0;
          if (!addEntry) {
            shistory->ReplaceEntry(index, mActiveEntry);
          }
        }

        if (loadFromSessionHistory) {
          // XXX Synchronize browsing context tree and session history tree?
          shistory->UpdateIndex();
        } else if (addEntry) {
          shistory->AddEntry(mActiveEntry, aPersist);
        }
      } else {
        // FIXME The old implementations adds it to the parent's mLSHE if there
        //       is one, need to figure out if that makes sense here (peterv
        //       doesn't think it would).
        if (loadFromSessionHistory) {
          if (mActiveEntry) {
            // mActiveEntry is null if we're loading iframes from session
            // history while also parent page is loading from session history.
            // In that case there isn't anything to sync.
            mActiveEntry->SyncTreesForSubframeNavigation(newActiveEntry, Top(),
                                                         this);
          }
          mActiveEntry = newActiveEntry;
          // FIXME UpdateIndex() here may update index too early (but even the
          //       old implementation seems to have similar issues).
          shistory->UpdateIndex();
        } else if (addEntry) {
          if (mActiveEntry) {
            if (LOAD_TYPE_HAS_FLAGS(
                    aLoadType, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY)) {
              // FIXME We need to make sure that when we create the info we
              //       make a copy of the shared state.
              mActiveEntry->ReplaceWith(*newActiveEntry);
            } else {
              // AddChildSHEntryHelper does update the index of the session
              // history!
              shistory->AddChildSHEntryHelper(mActiveEntry, newActiveEntry,
                                              Top(), aCloneEntryChildren);
              mActiveEntry = newActiveEntry;
            }
          } else {
            SessionHistoryEntry* parentEntry = GetParent()->mActiveEntry;
            // XXX What should happen if parent doesn't have mActiveEntry?
            //     Or can that even happen ever?
            if (parentEntry) {
              mActiveEntry = newActiveEntry;
              // FIXME Using IsInProcess for aUseRemoteSubframes isn't quite
              //       right, but aUseRemoteSubframes should be going away.
              parentEntry->AddChild(
                  mActiveEntry,
                  CreatedDynamically() ? -1 : GetParent()->IndexOf(this),
                  IsInProcess());
            }
          }
        }
      }

      ResetSHEntryHasUserInteractionCache();

      HistoryCommitIndexAndLength(aChangeID, caller);

      shistory->LogHistory();

      return;
    }
    // XXX Should the loading entries before [i] be removed?
  }
  // FIXME Should we throw an error if we don't find an entry for
  // aSessionHistoryEntryId?
}

static already_AddRefed<nsDocShellLoadState> CreateLoadInfo(
    SessionHistoryEntry* aEntry) {
  const SessionHistoryInfo& info = aEntry->Info();
  RefPtr<nsDocShellLoadState> loadState(new nsDocShellLoadState(info.GetURI()));
  info.FillLoadInfo(*loadState);
  UniquePtr<LoadingSessionHistoryInfo> loadingInfo;
  loadingInfo = MakeUnique<LoadingSessionHistoryInfo>(aEntry);
  loadState->SetLoadingSessionHistoryInfo(std::move(loadingInfo));

  return loadState.forget();
}

void CanonicalBrowsingContext::NotifyOnHistoryReload(
    bool aForceReload, bool& aCanReload,
    Maybe<RefPtr<nsDocShellLoadState>>& aLoadState,
    Maybe<bool>& aReloadActiveEntry) {
  MOZ_DIAGNOSTIC_ASSERT(!aLoadState);

  aCanReload = true;
  nsISHistory* shistory = GetSessionHistory();
  NS_ENSURE_TRUE_VOID(shistory);

  shistory->NotifyOnHistoryReload(&aCanReload);
  if (!aCanReload) {
    return;
  }

  if (mActiveEntry) {
    aLoadState.emplace(CreateLoadInfo(mActiveEntry));
    aReloadActiveEntry.emplace(true);
    if (aForceReload) {
      shistory->RemoveFrameEntries(mActiveEntry);
    }
  } else if (!mLoadingEntries.IsEmpty()) {
    const LoadingSessionHistoryEntry& loadingEntry =
        mLoadingEntries.LastElement();
    aLoadState.emplace(CreateLoadInfo(loadingEntry.mEntry));
    aReloadActiveEntry.emplace(false);
    if (aForceReload) {
      SessionHistoryEntry* entry =
          SessionHistoryEntry::GetByLoadId(loadingEntry.mLoadId);
      if (entry) {
        shistory->RemoveFrameEntries(entry);
      }
    }
  }

  if (aLoadState) {
    int32_t index = 0;
    int32_t requestedIndex = -1;
    int32_t length = 0;
    shistory->GetIndex(&index);
    shistory->GetRequestedIndex(&requestedIndex);
    shistory->GetCount(&length);
    aLoadState.ref()->SetLoadIsFromSessionHistory(
        requestedIndex >= 0 ? requestedIndex : index, length,
        aReloadActiveEntry.value());
  }
  // If we don't have an active entry and we don't have a loading entry then
  // the nsDocShell will create a load state based on its document.
}

void CanonicalBrowsingContext::SetActiveSessionHistoryEntry(
    const Maybe<nsPoint>& aPreviousScrollPos, SessionHistoryInfo* aInfo,
    uint32_t aLoadType, uint32_t aUpdatedCacheKey, const nsID& aChangeID) {
  nsISHistory* shistory = GetSessionHistory();
  if (!shistory) {
    return;
  }
  CallerWillNotifyHistoryIndexAndLengthChanges caller(shistory);

  RefPtr<SessionHistoryEntry> oldActiveEntry = mActiveEntry;
  if (aPreviousScrollPos.isSome() && oldActiveEntry) {
    oldActiveEntry->SetScrollPosition(aPreviousScrollPos.ref().x,
                                      aPreviousScrollPos.ref().y);
  }
  mActiveEntry = new SessionHistoryEntry(aInfo);
  mActiveEntry->SetDocshellID(GetHistoryID());
  mActiveEntry->AdoptBFCacheEntry(oldActiveEntry);
  if (aUpdatedCacheKey != 0) {
    mActiveEntry->SharedInfo()->mCacheKey = aUpdatedCacheKey;
  }

  if (IsTop()) {
    Maybe<int32_t> previousEntryIndex, loadedEntryIndex;
    shistory->AddToRootSessionHistory(
        true, oldActiveEntry, this, mActiveEntry, aLoadType,
        nsDocShell::ShouldAddToSessionHistory(aInfo->GetURI(), nullptr),
        &previousEntryIndex, &loadedEntryIndex);
  } else {
    if (oldActiveEntry) {
      shistory->AddChildSHEntryHelper(oldActiveEntry, mActiveEntry, Top(),
                                      true);
    } else if (GetParent() && GetParent()->mActiveEntry) {
      GetParent()->mActiveEntry->AddChild(
          mActiveEntry, CreatedDynamically() ? -1 : GetParent()->IndexOf(this),
          UseRemoteSubframes());
    }
  }

  ResetSHEntryHasUserInteractionCache();

  // FIXME Need to do the equivalent of EvictContentViewersOrReplaceEntry.
  HistoryCommitIndexAndLength(aChangeID, caller);

  static_cast<nsSHistory*>(shistory)->LogHistory();
}

void CanonicalBrowsingContext::ReplaceActiveSessionHistoryEntry(
    SessionHistoryInfo* aInfo) {
  if (!mActiveEntry) {
    return;
  }

  mActiveEntry->SetInfo(aInfo);
  // Notify children of the update
  nsSHistory* shistory = static_cast<nsSHistory*>(GetSessionHistory());
  if (shistory) {
    shistory->NotifyOnHistoryReplaceEntry();
    shistory->UpdateRootBrowsingContextState();
  }

  ResetSHEntryHasUserInteractionCache();

  // FIXME Need to do the equivalent of EvictContentViewersOrReplaceEntry.
}

void CanonicalBrowsingContext::RemoveDynEntriesFromActiveSessionHistoryEntry() {
  nsISHistory* shistory = GetSessionHistory();
  // In theory shistory can be null here if the method is called right after
  // CanonicalBrowsingContext::ReplacedBy call.
  NS_ENSURE_TRUE_VOID(shistory);
  nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(mActiveEntry);
  shistory->RemoveDynEntries(shistory->GetIndexOfEntry(root), mActiveEntry);
}

void CanonicalBrowsingContext::RemoveFromSessionHistory(const nsID& aChangeID) {
  nsSHistory* shistory = static_cast<nsSHistory*>(GetSessionHistory());
  if (shistory) {
    CallerWillNotifyHistoryIndexAndLengthChanges caller(shistory);
    nsCOMPtr<nsISHEntry> root = nsSHistory::GetRootSHEntry(mActiveEntry);
    bool didRemove;
    AutoTArray<nsID, 16> ids({GetHistoryID()});
    shistory->RemoveEntries(ids, shistory->GetIndexOfEntry(root), &didRemove);
    if (didRemove) {
      BrowsingContext* rootBC = shistory->GetBrowsingContext();
      if (rootBC) {
        if (!rootBC->IsInProcess()) {
          Unused << rootBC->Canonical()
                        ->GetContentParent()
                        ->SendDispatchLocationChangeEvent(rootBC);
        } else if (rootBC->GetDocShell()) {
          rootBC->GetDocShell()->DispatchLocationChangeEvent();
        }
      }
    }
    HistoryCommitIndexAndLength(aChangeID, caller);
  }
}

void CanonicalBrowsingContext::HistoryGo(
    int32_t aOffset, uint64_t aHistoryEpoch, bool aRequireUserInteraction,
    bool aUserActivation, Maybe<ContentParentId> aContentId,
    std::function<void(int32_t&&)>&& aResolver) {
  if (aRequireUserInteraction && aOffset != -1 && aOffset != 1) {
    NS_ERROR(
        "aRequireUserInteraction may only be used with an offset of -1 or 1");
    return;
  }

  nsSHistory* shistory = static_cast<nsSHistory*>(GetSessionHistory());
  if (!shistory) {
    return;
  }

  CheckedInt<int32_t> index = shistory->GetRequestedIndex() >= 0
                                  ? shistory->GetRequestedIndex()
                                  : shistory->Index();
  MOZ_LOG(gSHLog, LogLevel::Debug,
          ("HistoryGo(%d->%d) epoch %" PRIu64 "/id %" PRIu64, aOffset,
           (index + aOffset).value(), aHistoryEpoch,
           (uint64_t)(aContentId.isSome() ? aContentId.value() : 0)));

  while (true) {
    index += aOffset;
    if (!index.isValid()) {
      MOZ_LOG(gSHLog, LogLevel::Debug, ("Invalid index"));
      return;
    }

    // Check for user interaction if desired, except for the first and last
    // history entries. We compare with >= to account for the case where
    // aOffset >= length.
    if (!aRequireUserInteraction || index.value() >= shistory->Length() - 1 ||
        index.value() <= 0) {
      break;
    }
    if (shistory->HasUserInteractionAtIndex(index.value())) {
      break;
    }
  }

  // Implement aborting additional history navigations from within the same
  // event spin of the content process.

  uint64_t epoch;
  bool sameEpoch = false;
  Maybe<ContentParentId> id;
  shistory->GetEpoch(epoch, id);

  if (aContentId == id && epoch >= aHistoryEpoch) {
    sameEpoch = true;
    MOZ_LOG(gSHLog, LogLevel::Debug, ("Same epoch/id"));
  }
  // Don't update the epoch until we know if the target index is valid

  // GoToIndex checks that index is >= 0 and < length.
  nsTArray<nsSHistory::LoadEntryResult> loadResults;
  nsresult rv = shistory->GotoIndex(index.value(), loadResults, sameEpoch,
                                    aUserActivation);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gSHLog, LogLevel::Debug,
            ("Dropping HistoryGo - bad index or same epoch (not in same doc)"));
    return;
  }
  if (epoch < aHistoryEpoch || aContentId != id) {
    MOZ_LOG(gSHLog, LogLevel::Debug, ("Set epoch"));
    shistory->SetEpoch(aHistoryEpoch, aContentId);
  }
  aResolver(shistory->GetRequestedIndex());
  nsSHistory::LoadURIs(loadResults);
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

  if (mWebProgress) {
    RefPtr<BrowsingContextWebProgress> progress = mWebProgress;
    progress->ContextDiscarded();
  }

  if (IsTop()) {
    BackgroundSessionStorageManager::RemoveManager(Id());
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
  nsTHashSet<nsCString> uniqueSiteOrigins;

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
          uniqueSiteOrigins.Insert(siteOrigin);
        }
      }
    });
  }

  return uniqueSiteOrigins.Count();
}

void CanonicalBrowsingContext::UpdateMediaControlAction(
    const MediaControlAction& aAction) {
  if (IsDiscarded()) {
    return;
  }
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
    bool aRequireUserInteraction, bool aUserActivation) {
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
    docShell->GoBack(aRequireUserInteraction, aUserActivation);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch = Some(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoBack(this, cancelContentJSEpoch,
                             aRequireUserInteraction, aUserActivation);
  }
}
void CanonicalBrowsingContext::GoForward(
    const Optional<int32_t>& aCancelContentJSEpoch,
    bool aRequireUserInteraction, bool aUserActivation) {
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
    docShell->GoForward(aRequireUserInteraction, aUserActivation);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch.emplace(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoForward(this, cancelContentJSEpoch,
                                aRequireUserInteraction, aUserActivation);
  }
}
void CanonicalBrowsingContext::GoToIndex(
    int32_t aIndex, const Optional<int32_t>& aCancelContentJSEpoch,
    bool aUserActivation) {
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
    docShell->GotoIndex(aIndex, aUserActivation);
  } else if (ContentParent* cp = GetContentParent()) {
    Maybe<int32_t> cancelContentJSEpoch;
    if (aCancelContentJSEpoch.WasPassed()) {
      cancelContentJSEpoch.emplace(aCancelContentJSEpoch.Value());
    }
    Unused << cp->SendGoToIndex(this, aIndex, cancelContentJSEpoch,
                                aUserActivation);
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

void CanonicalBrowsingContext::PendingRemotenessChange::ProcessLaunched() {
  if (!mPromise) {
    return;
  }

  if (mContentParent) {
    // If our new content process is still unloading from a previous process
    // switch, wait for that unload to complete before continuing.
    auto found = mTarget->FindUnloadingHost(mContentParent->ChildID());
    if (found != mTarget->mUnloadingHosts.end()) {
      found->mCallbacks.AppendElement(
          [self = RefPtr{this}]() { self->ProcessReady(); });
      return;
    }
  }

  ProcessReady();
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

  // If this BrowsingContext is embedded within the parent process, perform the
  // process switch directly.
  nsresult rv = mTarget->IsTopContent() ? FinishTopContent() : FinishSubframe();
  if (NS_FAILED(rv)) {
    NS_WARNING("Error finishing PendingRemotenessChange!");
    Cancel(rv);
  } else {
    Clear();
  }
}

// Logic for finishing a toplevel process change embedded within the parent
// process. Due to frontend integration the logic differs substantially from
// subframe process switches, and is handled separately.
nsresult CanonicalBrowsingContext::PendingRemotenessChange::FinishTopContent() {
  MOZ_DIAGNOSTIC_ASSERT(mTarget->IsTop(),
                        "We shouldn't be trying to change the remoteness of "
                        "non-remote iframes");

  // While process switching, we need to check if any of our ancestors are
  // discarded or no longer current, in which case the process switch needs to
  // be aborted.
  RefPtr<CanonicalBrowsingContext> target(mTarget);
  if (target->IsDiscarded() || !target->AncestorsAreCurrent()) {
    return NS_ERROR_FAILURE;
  }

  Element* browserElement = target->GetEmbedderElement();
  if (!browserElement) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIBrowser> browser = browserElement->AsBrowser();
  if (!browser) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(browserElement);
  MOZ_RELEASE_ASSERT(frameLoaderOwner,
                     "embedder browser must be nsFrameLoaderOwner");

  // Tell frontend code that this browser element is about to change process.
  nsresult rv = browser->BeforeChangeRemoteness();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Some frontend code checks the value of the `remote` attribute on the
  // browser to determine if it is remote, so update the value.
  browserElement->SetAttr(kNameSpaceID_None, nsGkAtoms::remote,
                          mContentParent ? u"true"_ns : u"false"_ns,
                          /* notify */ true);

  // The process has been created, hand off to nsFrameLoaderOwner to finish
  // the process switch.
  ErrorResult error;
  frameLoaderOwner->ChangeRemotenessToProcess(mContentParent, mOptions,
                                              mSpecificGroup, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  // Tell frontend the load is done.
  bool loadResumed = false;
  rv = browser->FinishChangeRemoteness(mPendingSwitchId, &loadResumed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We did it! The process switch is complete.
  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  RefPtr<BrowserParent> newBrowser = frameLoader->GetBrowserParent();
  if (!newBrowser) {
    if (mContentParent) {
      // Failed to create the BrowserParent somehow! Abort the process switch
      // attempt.
      return NS_ERROR_UNEXPECTED;
    }

    if (!loadResumed) {
      RefPtr<nsDocShell> newDocShell = frameLoader->GetDocShell(error);
      if (error.Failed()) {
        return error.StealNSResult();
      }

      rv = newDocShell->ResumeRedirectedLoad(mPendingSwitchId,
                                             /* aHistoryIndex */ -1);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  } else if (!loadResumed) {
    newBrowser->ResumeLoad(mPendingSwitchId);
  }

  mPromise->Resolve(newBrowser, __func__);
  return NS_OK;
}

nsresult CanonicalBrowsingContext::PendingRemotenessChange::FinishSubframe() {
  MOZ_DIAGNOSTIC_ASSERT(!mOptions.mReplaceBrowsingContext,
                        "Cannot replace BC for subframe");
  MOZ_DIAGNOSTIC_ASSERT(!mTarget->IsTop());

  // While process switching, we need to check if any of our ancestors are
  // discarded or no longer current, in which case the process switch needs to
  // be aborted.
  RefPtr<CanonicalBrowsingContext> target(mTarget);
  if (target->IsDiscarded() || !target->AncestorsAreCurrent()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!mContentParent)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WindowGlobalParent> embedderWindow = target->GetParentWindowContext();
  if (NS_WARN_IF(!embedderWindow) || NS_WARN_IF(!embedderWindow->CanSend())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<BrowserParent> embedderBrowser = embedderWindow->GetBrowserParent();
  if (NS_WARN_IF(!embedderBrowser)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<BrowserParent> oldBrowser = target->GetBrowserParent();
  target->SetCurrentBrowserParent(nullptr);

  // If we were in a remote frame, trigger unloading of the remote window. The
  // previous BrowserParent is registered in `mUnloadingHosts` and will only be
  // cleared when the BrowserParent is fully destroyed.
  bool wasRemote = oldBrowser && oldBrowser->GetBrowsingContext() == target;
  if (wasRemote) {
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser != embedderBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser->IsDestroyed() ||
                          oldBrowser->GetBrowserBridgeParent());

    // `oldBrowser` will clear the `UnloadingHost` status once the actor has
    // been destroyed.
    if (oldBrowser->CanSend()) {
      target->StartUnloadingHost(oldBrowser->Manager()->ChildID());
      Unused << oldBrowser->SendWillChangeProcess();
      oldBrowser->Destroy();
    }
  }

  // Update which process is considered the current owner
  target->SetOwnerProcessId(mContentParent->ChildID());

  // If we're switching from remote to local, we don't need to create a
  // BrowserBridge, and can instead perform the switch directly.
  if (mContentParent == embedderBrowser->Manager()) {
    MOZ_DIAGNOSTIC_ASSERT(
        mPendingSwitchId,
        "We always have a PendingSwitchId, except for print-preview loads, "
        "which will never perform a process-switch to being in-process with "
        "their embedder");
    MOZ_DIAGNOSTIC_ASSERT(wasRemote,
                          "Attempt to process-switch from local to local?");

    target->SetCurrentBrowserParent(embedderBrowser);
    Unused << embedderWindow->SendMakeFrameLocal(target, mPendingSwitchId);
    mPromise->Resolve(embedderBrowser, __func__);
    return NS_OK;
  }

  // The BrowsingContext will be remote, either as an already-remote frame
  // changing processes, or as a local frame becoming remote. Construct a new
  // BrowserBridgeParent to host the remote content.
  target->SetCurrentBrowserParent(nullptr);

  MOZ_DIAGNOSTIC_ASSERT(target->UseRemoteTabs() && target->UseRemoteSubframes(),
                        "Not supported without fission");
  uint32_t chromeFlags = nsIWebBrowserChrome::CHROME_REMOTE_WINDOW |
                         nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  if (target->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }

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
    // If we've already destroyed our previous document, make a best-effort
    // attempt to recover from this failure and show the crashed tab UI. We only
    // do this in the previously-remote case, as previously in-process frames
    // will have their navigation cancelled, and will remain visible.
    if (wasRemote) {
      target->ShowSubframeCrashedUI(oldBrowser->GetBrowserBridgeParent());
    }
    return rv;
  }

  // Tell the embedder process a remoteness change is in-process. When this is
  // acknowledged, reset the in-flight ID if it used to be an in-process load.
  RefPtr<BrowserParent> newBrowser = bridge->GetBrowserParent();
  {
    // If we weren't remote, mark our embedder window browser as unloading until
    // our embedder process has acked our MakeFrameRemote message.
    Maybe<uint64_t> clearChildID;
    if (!wasRemote) {
      clearChildID = Some(embedderBrowser->Manager()->ChildID());
      target->StartUnloadingHost(*clearChildID);
    }
    auto callback = [target, clearChildID](auto&&) {
      if (clearChildID) {
        target->ClearUnloadingHost(*clearChildID);
      }
    };

    ManagedEndpoint<PBrowserBridgeChild> endpoint =
        embedderBrowser->OpenPBrowserBridgeEndpoint(bridge);
    MOZ_DIAGNOSTIC_ASSERT(endpoint.IsValid());
    embedderWindow->SendMakeFrameRemote(target, std::move(endpoint), tabId,
                                        newBrowser->GetLayersId(), callback,
                                        callback);
  }

  // Resume the pending load in our new process.
  if (mPendingSwitchId) {
    newBrowser->ResumeLoad(mPendingSwitchId);
  }

  // We did it! The process switch is complete.
  mPromise->Resolve(newBrowser, __func__);
  return NS_OK;
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
    uint64_t aPendingSwitchId, const RemotenessChangeOptions& aOptions)
    : mTarget(aTarget),
      mPromise(aPromise),
      mPendingSwitchId(aPendingSwitchId),
      mOptions(aOptions) {}

CanonicalBrowsingContext::PendingRemotenessChange::~PendingRemotenessChange() {
  MOZ_ASSERT(!mPromise && !mTarget && !mContentParent && !mSpecificGroup &&
                 !mPrepareToChangePromise,
             "should've already been Cancel() or Complete()-ed");
}

BrowserParent* CanonicalBrowsingContext::GetBrowserParent() const {
  return mCurrentBrowserParent;
}

void CanonicalBrowsingContext::SetCurrentBrowserParent(
    BrowserParent* aBrowserParent) {
  MOZ_DIAGNOSTIC_ASSERT(!mCurrentBrowserParent || !aBrowserParent,
                        "BrowsingContext already has a current BrowserParent!");
  MOZ_DIAGNOSTIC_ASSERT_IF(aBrowserParent, aBrowserParent->CanSend());
  MOZ_DIAGNOSTIC_ASSERT_IF(aBrowserParent,
                           aBrowserParent->Manager()->ChildID() == mProcessId);

  // BrowserParent must either be directly for this BrowsingContext, or the
  // manager out our embedder WindowGlobal.
  MOZ_DIAGNOSTIC_ASSERT_IF(
      aBrowserParent && aBrowserParent->GetBrowsingContext() != this,
      GetParentWindowContext() &&
          GetParentWindowContext()->Manager() == aBrowserParent);

  mCurrentBrowserParent = aBrowserParent;
}

RefPtr<CanonicalBrowsingContext::RemotenessPromise>
CanonicalBrowsingContext::ChangeRemoteness(
    const RemotenessChangeOptions& aOptions, uint64_t aPendingSwitchId) {
  MOZ_DIAGNOSTIC_ASSERT(IsContent(),
                        "cannot change the process of chrome contexts");
  MOZ_DIAGNOSTIC_ASSERT(
      IsTop() == IsEmbeddedInProcess(0),
      "toplevel content must be embedded in the parent process");
  MOZ_DIAGNOSTIC_ASSERT(!aOptions.mReplaceBrowsingContext || IsTop(),
                        "Cannot replace BrowsingContext for subframes");
  MOZ_DIAGNOSTIC_ASSERT(
      aOptions.mSpecificGroupId == 0 || aOptions.mReplaceBrowsingContext,
      "Cannot specify group ID unless replacing BC");
  MOZ_DIAGNOSTIC_ASSERT(aPendingSwitchId || !IsTop(),
                        "Should always have aPendingSwitchId for top-level "
                        "frames");

  if (!AncestorsAreCurrent()) {
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

  if (aOptions.mRemoteType.IsEmpty() && (!IsTop() || !GetEmbedderElement())) {
    NS_WARNING("Cannot load non-remote subframes");
    return RemotenessPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Cancel ongoing remoteness changes.
  if (mPendingRemotenessChange) {
    mPendingRemotenessChange->Cancel(NS_ERROR_ABORT);
    MOZ_DIAGNOSTIC_ASSERT(!mPendingRemotenessChange, "Should have cleared");
  }

  auto promise = MakeRefPtr<RemotenessPromise::Private>(__func__);
  RefPtr<PendingRemotenessChange> change =
      new PendingRemotenessChange(this, promise, aPendingSwitchId, aOptions);
  mPendingRemotenessChange = change;

  // If a specific BrowsingContextGroup ID was specified for this load, make
  // sure to keep it alive until the process switch is completed.
  if (aOptions.mSpecificGroupId) {
    change->mSpecificGroup =
        BrowsingContextGroup::GetOrCreate(aOptions.mSpecificGroupId);
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

  // Switching a subframe to be local within it's embedding process.
  RefPtr<BrowserParent> embedderBrowser =
      embedderWindowGlobal->GetBrowserParent();
  if (embedderBrowser &&
      aOptions.mRemoteType == embedderBrowser->Manager()->GetRemoteType()) {
    MOZ_DIAGNOSTIC_ASSERT(
        aPendingSwitchId,
        "We always have a PendingSwitchId, except for print-preview loads, "
        "which will never perform a process-switch to being in-process with "
        "their embedder");
    MOZ_DIAGNOSTIC_ASSERT(!aOptions.mReplaceBrowsingContext);
    MOZ_DIAGNOSTIC_ASSERT(!aOptions.mRemoteType.IsEmpty());
    MOZ_DIAGNOSTIC_ASSERT(!change->mPrepareToChangePromise);
    MOZ_DIAGNOSTIC_ASSERT(!change->mSpecificGroup);

    // Switching to local, so we don't need to create a new process, and will
    // instead use our embedder process.
    change->mContentParent = embedderBrowser->Manager();
    change->mContentParent->AddKeepAlive();
    change->ProcessLaunched();
    return promise.forget();
  }

  // Switching to the parent process.
  if (aOptions.mRemoteType.IsEmpty()) {
    change->ProcessLaunched();
    return promise.forget();
  }

  // Try to predict which BrowsingContextGroup will be used for the final load
  // in this BrowsingContext. This has to be accurate if switching into an
  // existing group, as it will control what pool of processes will be used
  // for process selection.
  //
  // It's _technically_ OK to provide a group here if we're actually going to
  // switch into a brand new group, though it's sub-optimal, as it can
  // restrict the set of processes we're using.
  BrowsingContextGroup* finalGroup =
      aOptions.mReplaceBrowsingContext ? change->mSpecificGroup.get() : Group();

  change->mContentParent = ContentParent::GetNewOrUsedLaunchingBrowserProcess(
      /* aRemoteType = */ aOptions.mRemoteType,
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
      [change](ContentParent*) { change->ProcessLaunched(); },
      [change](LaunchError) { change->Cancel(NS_ERROR_FAILURE); });
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

bool CanonicalBrowsingContext::HasCreatedMediaController() const {
  return !!mTabMediaController;
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

  // Session-history-in-parent implementation relies currently on getting a
  // round trip through a child process.
  if (aLoadState->LoadIsFromSessionHistory()) {
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

already_AddRefed<nsIURI> CanonicalBrowsingContext::GetCurrentURI() const {
  nsCOMPtr<nsIURI> currentURI;
  if (nsIDocShell* docShell = GetDocShell()) {
    MOZ_ALWAYS_SUCCEEDS(
        nsDocShell::Cast(docShell)->GetCurrentURI(getter_AddRefs(currentURI)));
  } else {
    currentURI = mCurrentRemoteURI;
  }
  return currentURI.forget();
}

void CanonicalBrowsingContext::SetCurrentRemoteURI(nsIURI* aCurrentRemoteURI) {
  MOZ_ASSERT(!GetDocShell());
  mCurrentRemoteURI = aCurrentRemoteURI;
}

void CanonicalBrowsingContext::ResetSHEntryHasUserInteractionCache() {
  WindowContext* topWc = GetTopWindowContext();
  if (topWc && !topWc->IsDiscarded()) {
    MOZ_ALWAYS_SUCCEEDS(topWc->SetSHEntryHasUserInteraction(false));
  }
}

void CanonicalBrowsingContext::HistoryCommitIndexAndLength() {
  nsID changeID = {};
  CallerWillNotifyHistoryIndexAndLengthChanges caller(nullptr);
  HistoryCommitIndexAndLength(changeID, caller);
}
void CanonicalBrowsingContext::HistoryCommitIndexAndLength(
    const nsID& aChangeID,
    const CallerWillNotifyHistoryIndexAndLengthChanges& aProofOfCaller) {
  if (!IsTop()) {
    Cast(Top())->HistoryCommitIndexAndLength(aChangeID, aProofOfCaller);
    return;
  }

  nsISHistory* shistory = GetSessionHistory();
  if (!shistory) {
    return;
  }
  int32_t index = 0;
  shistory->GetIndex(&index);
  int32_t length = shistory->GetCount();

  GetChildSessionHistory()->SetIndexAndLength(index, length, aChangeID);

  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendHistoryCommitIndexAndLength(this, index, length,
                                                       aChangeID);
  });
}

void CanonicalBrowsingContext::ResetScalingZoom() {
  // This currently only ever gets called in the parent process, and we
  // pass the message on to the WindowGlobalChild for the rootmost browsing
  // context.
  if (WindowGlobalParent* topWindow = GetTopWindowContext()) {
    Unused << topWindow->SendResetScalingZoom();
  }
}

void CanonicalBrowsingContext::SetRestoreData(SessionStoreRestoreData* aData,
                                              ErrorResult& aError) {
  MOZ_DIAGNOSTIC_ASSERT(aData);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
    return;
  }

  if (NS_WARN_IF(NS_FAILED(SetHasRestoreData(true)))) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mRestoreState = new RestoreState();
  mRestoreState->mData = aData;
  mRestoreState->mPromise = promise;
}

already_AddRefed<Promise> CanonicalBrowsingContext::GetRestorePromise() {
  if (mRestoreState) {
    return do_AddRef(mRestoreState->mPromise);
  }
  return nullptr;
}

void CanonicalBrowsingContext::ClearRestoreState() {
  if (!mRestoreState) {
    MOZ_DIAGNOSTIC_ASSERT(!GetHasRestoreData());
    return;
  }
  if (mRestoreState->mPromise) {
    mRestoreState->mPromise->MaybeRejectWithUndefined();
  }
  mRestoreState = nullptr;
  MOZ_ALWAYS_SUCCEEDS(SetHasRestoreData(false));
}

void CanonicalBrowsingContext::RequestRestoreTabContent(
    WindowGlobalParent* aWindow) {
  MOZ_DIAGNOSTIC_ASSERT(IsTop());

  if (IsDiscarded() || !mRestoreState || !mRestoreState->mData) {
    return;
  }

  CanonicalBrowsingContext* context = aWindow->GetBrowsingContext();
  MOZ_DIAGNOSTIC_ASSERT(!context->IsDiscarded());

  RefPtr<SessionStoreRestoreData> data =
      mRestoreState->mData->FindDataForChild(context);

  if (context->IsTop()) {
    MOZ_DIAGNOSTIC_ASSERT(context == this);

    // We need to wait until the appropriate load event has fired before we
    // can "complete" the restore process, so if we're holding an empty data
    // object, just resolve the promise immediately.
    if (mRestoreState->mData->IsEmpty()) {
      MOZ_DIAGNOSTIC_ASSERT(!data || data->IsEmpty());
      mRestoreState->Resolve();
      ClearRestoreState();
      return;
    }

    // Since we're following load event order, we'll only arrive here for a
    // toplevel context after we've already sent down data for all child frames,
    // so it's safe to clear this reference now. The completion callback below
    // relies on the mData field being null to determine if all requests have
    // been sent out.
    mRestoreState->ClearData();
    MOZ_ALWAYS_SUCCEEDS(SetHasRestoreData(false));
  }

  if (data && !data->IsEmpty()) {
    auto onTabRestoreComplete = [self = RefPtr{this},
                                 state = RefPtr{mRestoreState}](auto) {
      state->mResolves++;
      if (!state->mData && state->mRequests == state->mResolves) {
        state->Resolve();
        if (state == self->mRestoreState) {
          self->ClearRestoreState();
        }
      }
    };

    mRestoreState->mRequests++;

    if (data->CanRestoreInto(aWindow->GetDocumentURI())) {
      if (!aWindow->IsInProcess()) {
        aWindow->SendRestoreTabContent(data, onTabRestoreComplete,
                                       onTabRestoreComplete);
        return;
      }
      data->RestoreInto(context);
    }

    // This must be called both when we're doing an in-process restore, and when
    // we didn't do a restore at all due to a URL mismatch.
    onTabRestoreComplete(true);
  }
}

void CanonicalBrowsingContext::RestoreState::Resolve() {
  MOZ_DIAGNOSTIC_ASSERT(mPromise);
  mPromise->MaybeResolveWithUndefined();
  mPromise = nullptr;
}

void CanonicalBrowsingContext::SetContainerFeaturePolicy(
    FeaturePolicy* aContainerFeaturePolicy) {
  mContainerFeaturePolicy = aContainerFeaturePolicy;

  if (WindowGlobalParent* current = GetCurrentWindowGlobal()) {
    Unused << current->SendSetContainerFeaturePolicy(mContainerFeaturePolicy);
  }
}

void CanonicalBrowsingContext::SetCrossGroupOpenerId(uint64_t aOpenerId) {
  MOZ_DIAGNOSTIC_ASSERT(IsTopContent());
  MOZ_DIAGNOSTIC_ASSERT(mCrossGroupOpenerId == 0,
                        "Can only set CrossGroupOpenerId once");
  mCrossGroupOpenerId = aOpenerId;
}

auto CanonicalBrowsingContext::FindUnloadingHost(uint64_t aChildID)
    -> nsTArray<UnloadingHost>::iterator {
  return std::find_if(
      mUnloadingHosts.begin(), mUnloadingHosts.end(),
      [&](const auto& host) { return host.mChildID == aChildID; });
}

void CanonicalBrowsingContext::ClearUnloadingHost(uint64_t aChildID) {
  // Notify any callbacks which were waiting for the host to finish unloading
  // that it has.
  auto found = FindUnloadingHost(aChildID);
  if (found != mUnloadingHosts.end()) {
    auto callbacks = std::move(found->mCallbacks);
    mUnloadingHosts.RemoveElementAt(found);
    for (const auto& callback : callbacks) {
      callback();
    }
  }
}

void CanonicalBrowsingContext::StartUnloadingHost(uint64_t aChildID) {
  MOZ_DIAGNOSTIC_ASSERT(FindUnloadingHost(aChildID) == mUnloadingHosts.end());
  mUnloadingHosts.AppendElement(UnloadingHost{aChildID, {}});
}

void CanonicalBrowsingContext::BrowserParentDestroyed(
    BrowserParent* aBrowserParent, bool aAbnormalShutdown) {
  ClearUnloadingHost(aBrowserParent->Manager()->ChildID());

  // Handling specific to when the current BrowserParent has been destroyed.
  if (mCurrentBrowserParent == aBrowserParent) {
    mCurrentBrowserParent = nullptr;

    // If this BrowserParent is for a subframe, attempt to recover from a
    // subframe crash by rendering the subframe crashed page in the embedding
    // content.
    if (aAbnormalShutdown) {
      ShowSubframeCrashedUI(aBrowserParent->GetBrowserBridgeParent());
    }
  }
}

void CanonicalBrowsingContext::ShowSubframeCrashedUI(
    BrowserBridgeParent* aBridge) {
  if (!aBridge || IsDiscarded() || !aBridge->CanSend()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!aBridge->GetBrowsingContext() ||
                        aBridge->GetBrowsingContext() == this);

  // There is no longer a current inner window within this
  // BrowsingContext, update the `CurrentInnerWindowId` field to reflect
  // this.
  MOZ_ALWAYS_SUCCEEDS(SetCurrentInnerWindowId(0));

  // The owning process will now be the embedder to render the subframe
  // crashed page, switch ownership back over.
  SetOwnerProcessId(aBridge->Manager()->Manager()->ChildID());
  SetCurrentBrowserParent(aBridge->Manager());

  Unused << aBridge->SendSubFrameCrashed();
}

static void LogBFCacheBlockingForDoc(BrowsingContext* aBrowsingContext,
                                     uint16_t aBFCacheCombo, bool aIsSubDoc) {
  if (aIsSubDoc) {
    nsAutoCString uri("[no uri]");
    nsCOMPtr<nsIURI> currentURI =
        aBrowsingContext->Canonical()->GetCurrentURI();
    if (currentURI) {
      uri = currentURI->GetSpecOrDefault();
    }
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
            (" ** Blocked for document %s", uri.get()));
  }
  if (aBFCacheCombo & BFCacheStatus::EVENT_HANDLING_SUPPRESSED) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
            (" * event handling suppression"));
  }
  if (aBFCacheCombo & BFCacheStatus::SUSPENDED) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * suspended Window"));
  }
  if (aBFCacheCombo & BFCacheStatus::UNLOAD_LISTENER) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
            (" * beforeunload or unload listener"));
  }
  if (aBFCacheCombo & BFCacheStatus::REQUEST) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * requests in the loadgroup"));
  }
  if (aBFCacheCombo & BFCacheStatus::ACTIVE_GET_USER_MEDIA) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * GetUserMedia"));
  }
  if (aBFCacheCombo & BFCacheStatus::ACTIVE_PEER_CONNECTION) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * PeerConnection"));
  }
  if (aBFCacheCombo & BFCacheStatus::CONTAINS_EME_CONTENT) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * EME content"));
  }
  if (aBFCacheCombo & BFCacheStatus::CONTAINS_MSE_CONTENT) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * MSE use"));
  }
  if (aBFCacheCombo & BFCacheStatus::HAS_ACTIVE_SPEECH_SYNTHESIS) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * Speech use"));
  }
  if (aBFCacheCombo & BFCacheStatus::HAS_USED_VR) {
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * used VR"));
  }
}

bool CanonicalBrowsingContext::AllowedInBFCache(
    const Maybe<uint64_t>& aChannelId) {
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gSHIPBFCacheLog, LogLevel::Debug))) {
    nsAutoCString uri("[no uri]");
    nsCOMPtr<nsIURI> currentURI = GetCurrentURI();
    if (currentURI) {
      uri = currentURI->GetSpecOrDefault();
    }
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, ("Checking %s", uri.get()));
  }

  if (IsInProcess()) {
    return false;
  }

  uint16_t bfcacheCombo = 0;
  if (Group()->Toplevels().Length() > 1) {
    bfcacheCombo |= BFCacheStatus::NOT_ONLY_TOPLEVEL_IN_BCG;
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
            (" * auxiliary BrowsingContexts"));
  }

  // There are not a lot of about:* pages that are allowed to load in
  // subframes, so it's OK to allow those few about:* pages enter BFCache.
  MOZ_ASSERT(IsTop(), "Trying to put a non top level BC into BFCache");

  nsCOMPtr<nsIURI> currentURI = GetCurrentURI();
  // Exempt about:* pages from bfcache, with the exception of about:blank
  if (currentURI && currentURI->SchemeIs("about") &&
      !currentURI->GetSpecOrDefault().EqualsLiteral("about:blank")) {
    bfcacheCombo |= BFCacheStatus::ABOUT_PAGE;
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug, (" * about:* page"));
  }

  // For telemetry we're collecting all the flags for all the BCs hanging
  // from this top-level BC.
  PreOrderWalk([&](BrowsingContext* aBrowsingContext) {
    WindowGlobalParent* wgp =
        aBrowsingContext->Canonical()->GetCurrentWindowGlobal();
    uint16_t subDocBFCacheCombo = wgp ? wgp->GetBFCacheStatus() : 0;
    if (wgp) {
      const Maybe<uint64_t>& singleChannelId = wgp->GetSingleChannelId();
      if (singleChannelId.isSome()) {
        if (singleChannelId.value() == 0 || aChannelId.isNothing() ||
            singleChannelId.value() != aChannelId.value()) {
          subDocBFCacheCombo |= BFCacheStatus::REQUEST;
        }
      }
    }

    if (MOZ_UNLIKELY(MOZ_LOG_TEST(gSHIPBFCacheLog, LogLevel::Debug))) {
      LogBFCacheBlockingForDoc(aBrowsingContext, subDocBFCacheCombo,
                               aBrowsingContext != this);
    }

    bfcacheCombo |= subDocBFCacheCombo;
  });

  nsDocShell::ReportBFCacheComboTelemetry(bfcacheCombo);
  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gSHIPBFCacheLog, LogLevel::Debug))) {
    nsAutoCString uri("[no uri]");
    nsCOMPtr<nsIURI> currentURI = GetCurrentURI();
    if (currentURI) {
      uri = currentURI->GetSpecOrDefault();
    }
    MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
            (" +> %s %s be blocked from going into the BFCache", uri.get(),
             bfcacheCombo == 0 ? "shouldn't" : "should"));
  }

  if (StaticPrefs::docshell_shistory_bfcache_allow_unload_listeners()) {
    bfcacheCombo &= ~BFCacheStatus::UNLOAD_LISTENER;
  }

  return bfcacheCombo == 0;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanonicalBrowsingContext, BrowsingContext,
                                   mSessionHistory, mContainerFeaturePolicy,
                                   mCurrentBrowserParent)

NS_IMPL_ADDREF_INHERITED(CanonicalBrowsingContext, BrowsingContext)
NS_IMPL_RELEASE_INHERITED(CanonicalBrowsingContext, BrowsingContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanonicalBrowsingContext)
NS_INTERFACE_MAP_END_INHERITING(BrowsingContext)

}  // namespace dom
}  // namespace mozilla

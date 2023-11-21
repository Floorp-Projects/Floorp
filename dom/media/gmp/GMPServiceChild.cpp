/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceChild.h"

#include "GMPLog.h"
#include "GMPParent.h"
#include "base/task.h"
#include "mozIGeckoMediaPluginChromeService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsReadableUtils.h"
#include "nsXPCOMPrivate.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPServiceChild"

already_AddRefed<GeckoMediaPluginServiceChild>
GeckoMediaPluginServiceChild::GetSingleton() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  RefPtr<GeckoMediaPluginService> service(
      GeckoMediaPluginService::GetGeckoMediaPluginService());
#ifdef DEBUG
  if (service) {
    nsCOMPtr<mozIGeckoMediaPluginChromeService> chromeService;
    CallQueryInterface(service.get(), getter_AddRefs(chromeService));
    MOZ_ASSERT(!chromeService);
  }
#endif
  return service.forget().downcast<GeckoMediaPluginServiceChild>();
}

nsresult GeckoMediaPluginServiceChild::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  GMP_LOG_DEBUG("%s::%s", __CLASS__, __FUNCTION__);

  nsresult rv = AddShutdownBlocker();
  if (NS_FAILED(rv)) {
    MOZ_ASSERT_UNREACHABLE(
        "We expect xpcom to be live when calling this, so we should be able to "
        "add a blocker");
    GMP_LOG_DEBUG("%s::%s failed to add shutdown blocker!", __CLASS__,
                  __FUNCTION__);
    return rv;
  }

  return GeckoMediaPluginService::Init();
}

NS_IMPL_ISUPPORTS_INHERITED(GeckoMediaPluginServiceChild,
                            GeckoMediaPluginService, nsIAsyncShutdownBlocker)

// Used to identify blockers that we put in place.
static const nsLiteralString kShutdownBlockerName =
    u"GeckoMediaPluginServiceChild: shutdown"_ns;

// nsIAsyncShutdownBlocker members
NS_IMETHODIMP
GeckoMediaPluginServiceChild::GetName(nsAString& aName) {
  aName = kShutdownBlockerName;
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::GetState(nsIPropertyBag**) { return NS_OK; }

NS_IMETHODIMP
GeckoMediaPluginServiceChild::BlockShutdown(nsIAsyncShutdownClient*) {
  MOZ_ASSERT(NS_IsMainThread());
  GMP_LOG_DEBUG("%s::%s", __CLASS__, __FUNCTION__);

  mXPCOMWillShutdown = true;

  MutexAutoLock lock(mMutex);
  Unused << NS_WARN_IF(NS_FAILED(mGMPThread->Dispatch(
      NewRunnableMethod("GeckoMediaPluginServiceChild::BeginShutdown", this,
                        &GeckoMediaPluginServiceChild::BeginShutdown))));
  return NS_OK;
}
// End nsIAsyncShutdownBlocker members

GeckoMediaPluginServiceChild::~GeckoMediaPluginServiceChild() {
  MOZ_ASSERT(!mServiceChild);
}

RefPtr<GetGMPContentParentPromise>
GeckoMediaPluginServiceChild::GetContentParent(
    GMPCrashHelper* aHelper, const NodeIdVariant& aNodeIdVariant,
    const nsACString& aAPI, const nsTArray<nsCString>& aTags) {
  AssertOnGMPThread();
  MOZ_ASSERT(!mShuttingDownOnGMPThread,
             "Should not be called if GMPThread is shutting down!");

  MozPromiseHolder<GetGMPContentParentPromise>* rawHolder =
      new MozPromiseHolder<GetGMPContentParentPromise>();
  RefPtr<GetGMPContentParentPromise> promise = rawHolder->Ensure(__func__);
  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());

  nsCString api(aAPI);
  RefPtr<GMPCrashHelper> helper(aHelper);
  RefPtr<GeckoMediaPluginServiceChild> self(this);

  mPendingGetContentParents += 1;

  GetServiceChild()->Then(
      thread, __func__,
      [nodeIdVariant = aNodeIdVariant, self, api, tags = aTags.Clone(), helper,
       rawHolder](GMPServiceChild* child) {
        nsTArray<base::ProcessId> alreadyBridgedTo;

        // We want to force the content process to keep all of our
        // GMPContentParent IPDL objects alive while we wait to resolve which
        // process can satisfy our request. This avoids a race condition where
        // the last dependency is released before we can acquire our own.
        auto* rawBlockers =
            new nsTArray<RefPtr<GMPContentParentCloseBlocker>>();
        child->GetAndBlockAlreadyBridgedTo(alreadyBridgedTo, *rawBlockers);

        child->SendLaunchGMP(
            nodeIdVariant, api, tags, alreadyBridgedTo,
            [rawHolder, self, helper, rawBlockers,
             child = RefPtr{child}](GMPLaunchResult&& aResult) {
              UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>> holder(
                  rawHolder);
              UniquePtr<nsTArray<RefPtr<GMPContentParentCloseBlocker>>>
                  blockers(rawBlockers);
              if (helper && aResult.pluginId()) {
                // Note: Even if the launch failed, we need to connect the crash
                // helper so that if the launch failed due to the plugin
                // crashing, we can report the crash via the crash reporter. The
                // crash handling notification will arrive shortly if the launch
                // failed due to the plugin crashing.
                self->ConnectCrashHelper(aResult.pluginId(), helper);
              }

              if (NS_WARN_IF(NS_FAILED(aResult.result()))) {
                MediaResult error(
                    aResult.result(),
                    nsPrintfCString(
                        "GeckoMediaPluginServiceChild::GetContentParent "
                        "SendLaunchGMPForNodeId failed with description (%s)",
                        aResult.errorDescription().get()));

                GMP_LOG_DEBUG("%s failed to launch GMP with error: %s",
                              __CLASS__, aResult.errorDescription().get());
                self->mPendingGetContentParents -= 1;
                self->RemoveShutdownBlockerIfNeeded();

                holder->Reject(error, __func__);
                return;
              }

              // If we didn't explicitly fail, we should have been told about a
              // process running.
              MOZ_ASSERT(aResult.pid() != base::kInvalidProcessId);

              // It is possible the GMP process may have terminated before we
              // were able to process this result. In that case, we should just
              // fail as normal as the initialization of the IPDL objects would
              // have just failed anyways and produced the same result.
              bool contains = child->HasAlreadyBridgedTo(aResult.pid());
              if (NS_WARN_IF(!contains && !aResult.endpoint().IsValid())) {
                MediaResult error(
                    aResult.result(),
                    "GeckoMediaPluginServiceChild::GetContentParent "
                    "SendLaunchGMPForNodeId failed with process exit"_ns);

                GMP_LOG_DEBUG("%s failed to launch GMP with process exit",
                              __CLASS__);
                self->mPendingGetContentParents -= 1;
                self->RemoveShutdownBlockerIfNeeded();

                holder->Reject(error, __func__);
                return;
              }

              RefPtr<GMPContentParent> parent =
                  child->GetBridgedGMPContentParent(
                      aResult.pid(), std::move(aResult.endpoint()));
              if (!contains) {
                parent->SetDisplayName(aResult.displayName());
                parent->SetPluginId(aResult.pluginId());
                parent->SetPluginType(aResult.pluginType());
              }

              // The content parent is no longer pending.
              self->mPendingGetContentParents -= 1;
              MOZ_ASSERT(child->HaveContentParents(),
                         "We should have at least one content parent!");
              // We don't check if we need to remove the shutdown blocker here
              // as we should always have at least one live content parent.

              RefPtr<GMPContentParentCloseBlocker> blocker(
                  new GMPContentParentCloseBlocker(parent));
              holder->Resolve(blocker, __func__);
            },
            [rawHolder, self, helper,
             rawBlockers](const ipc::ResponseRejectReason&) {
              UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>> holder(
                  rawHolder);
              UniquePtr<nsTArray<RefPtr<GMPContentParentCloseBlocker>>>
                  blockers(rawBlockers);
              MediaResult error(
                  NS_ERROR_FAILURE,
                  "GeckoMediaPluginServiceChild::GetContentParent "
                  "SendLaunchGMPForNodeId failed with IPC error"_ns);

              GMP_LOG_DEBUG("%s failed to launch GMP with IPC error",
                            __CLASS__);
              self->mPendingGetContentParents -= 1;
              self->RemoveShutdownBlockerIfNeeded();

              holder->Reject(error, __func__);
            });
      },
      [self, rawHolder](MediaResult result) {
        self->mPendingGetContentParents -= 1;
        self->RemoveShutdownBlockerIfNeeded();
        UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>> holder(
            rawHolder);
        holder->Reject(result, __func__);
      });

  return promise;
}

typedef mozilla::dom::GMPCapabilityData GMPCapabilityData;
typedef mozilla::dom::GMPAPITags GMPAPITags;

struct GMPCapabilityAndVersion {
  explicit GMPCapabilityAndVersion(const GMPCapabilityData& aCapabilities)
      : mName(aCapabilities.name()), mVersion(aCapabilities.version()) {
    for (const GMPAPITags& tags : aCapabilities.capabilities()) {
      GMPCapability cap;
      cap.mAPIName = tags.api();
      for (const nsACString& tag : tags.tags()) {
        cap.mAPITags.AppendElement(tag);
      }
      mCapabilities.AppendElement(std::move(cap));
    }
  }

  nsCString ToString() const {
    nsCString s;
    s.Append(mName);
    s.AppendLiteral(" version=");
    s.Append(mVersion);
    s.AppendLiteral(" tags=[");
    StringJoinAppend(s, " "_ns, mCapabilities,
                     [](auto& tags, const GMPCapability& cap) {
                       tags.Append(cap.mAPIName);
                       for (const nsACString& tag : cap.mAPITags) {
                         tags.AppendLiteral(":");
                         tags.Append(tag);
                       }
                     });
    s.AppendLiteral("]");
    return s;
  }

  nsCString mName;
  nsCString mVersion;
  nsTArray<GMPCapability> mCapabilities;
};

StaticMutex sGMPCapabilitiesMutex;
StaticAutoPtr<nsTArray<GMPCapabilityAndVersion>> sGMPCapabilities;

static auto GMPCapabilitiesToString() {
  return StringJoin(", "_ns, *sGMPCapabilities,
                    [](nsACString& dest, const GMPCapabilityAndVersion& gmp) {
                      dest.Append(gmp.ToString());
                    });
}

/* static */
void GeckoMediaPluginServiceChild::UpdateGMPCapabilities(
    nsTArray<GMPCapabilityData>&& aCapabilities) {
  {
    // The mutex should unlock before sending the "gmp-changed" observer
    // service notification.
    StaticMutexAutoLock lock(sGMPCapabilitiesMutex);
    if (!sGMPCapabilities) {
      sGMPCapabilities = new nsTArray<GMPCapabilityAndVersion>();
      ClearOnShutdown(&sGMPCapabilities);
    }
    sGMPCapabilities->Clear();
    for (const GMPCapabilityData& plugin : aCapabilities) {
      sGMPCapabilities->AppendElement(GMPCapabilityAndVersion(plugin));
    }

    GMP_LOG_DEBUG("%s::%s {%s}", __CLASS__, __FUNCTION__,
                  GMPCapabilitiesToString().get());
  }

  // Fire a notification so that any MediaKeySystemAccess
  // requests waiting on a CDM to download will retry.
  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  if (obsService) {
    obsService->NotifyObservers(nullptr, "gmp-changed", nullptr);
  }
}

void GeckoMediaPluginServiceChild::BeginShutdown() {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: mServiceChild=%p,", __CLASS__, __FUNCTION__,
                mServiceChild.get());
  // It's possible this gets called twice if the parent sends us a message to
  // shutdown and we block shutdown in content in close proximity.
  mShuttingDownOnGMPThread = true;
  RemoveShutdownBlockerIfNeeded();
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::HasPluginForAPI(const nsACString& aAPI,
                                              const nsTArray<nsCString>& aTags,
                                              bool* aHasPlugin) {
  StaticMutexAutoLock lock(sGMPCapabilitiesMutex);
  if (!sGMPCapabilities) {
    *aHasPlugin = false;
    return NS_OK;
  }

  nsCString api(aAPI);
  for (const GMPCapabilityAndVersion& plugin : *sGMPCapabilities) {
    if (GMPCapability::Supports(plugin.mCapabilities, api, aTags)) {
      *aHasPlugin = true;
      return NS_OK;
    }
  }

  *aHasPlugin = false;
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::FindPluginDirectoryForAPI(
    const nsACString& aAPI, const nsTArray<nsCString>& aTags,
    nsIFile** aDirectory) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::GetNodeId(
    const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
    const nsAString& aGMPName, UniquePtr<GetNodeIdCallback>&& aCallback) {
  AssertOnGMPThread();

  GetNodeIdCallback* rawCallback = aCallback.release();
  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());
  nsString origin(aOrigin);
  nsString topLevelOrigin(aTopLevelOrigin);
  nsString gmpName(aGMPName);
  GetServiceChild()->Then(
      thread, __func__,
      [rawCallback, origin, topLevelOrigin, gmpName](GMPServiceChild* child) {
        child->SendGetGMPNodeId(
            origin, topLevelOrigin, gmpName,
            [rawCallback](nsCString&& aId) {
              UniquePtr<GetNodeIdCallback> callback(rawCallback);
              callback->Done(NS_OK, aId);
            },
            [rawCallback](const ipc::ResponseRejectReason&) {
              UniquePtr<GetNodeIdCallback> callback(rawCallback);
              callback->Done(NS_ERROR_FAILURE, ""_ns);
            });
      },
      [rawCallback](nsresult rv) {
        UniquePtr<GetNodeIdCallback> callback(rawCallback);
        callback->Done(NS_ERROR_FAILURE, ""_ns);
      });

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aSomeData) {
  MOZ_ASSERT(NS_IsMainThread());
  GMP_LOG_DEBUG("%s::%s: aTopic=%s", __CLASS__, __FUNCTION__, aTopic);
  if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    if (mServiceChild) {
      MutexAutoLock lock(mMutex);
      mozilla::SyncRunnable::DispatchToThread(
          mGMPThread,
          WrapRunnable(mServiceChild.get(), &PGMPServiceChild::Close));
      mServiceChild = nullptr;
    }
    ShutdownGMPThread();
  }

  return NS_OK;
}

RefPtr<GeckoMediaPluginServiceChild::GetServiceChildPromise>
GeckoMediaPluginServiceChild::GetServiceChild() {
  AssertOnGMPThread();

  if (!mServiceChild) {
    if (mShuttingDownOnGMPThread) {
      // We have begun shutdown. Don't allow a new connection to the main
      // process to be instantiated. This also prevents new plugins being
      // instantiated.
      return GetServiceChildPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
    }
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (!contentChild) {
      return GetServiceChildPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
    }
    MozPromiseHolder<GetServiceChildPromise>* holder =
        mGetServiceChildPromises.AppendElement();
    RefPtr<GetServiceChildPromise> promise = holder->Ensure(__func__);
    if (mGetServiceChildPromises.Length() == 1) {
      nsCOMPtr<nsIRunnable> r =
          WrapRunnable(contentChild, &dom::ContentChild::SendCreateGMPService);
      SchedulerGroup::Dispatch(r.forget());
    }
    return promise;
  }
  return GetServiceChildPromise::CreateAndResolve(mServiceChild.get(),
                                                  __func__);
}

void GeckoMediaPluginServiceChild::SetServiceChild(
    RefPtr<GMPServiceChild>&& aServiceChild) {
  AssertOnGMPThread();
  MOZ_ASSERT(!mServiceChild, "Should not already have service child!");
  GMP_LOG_DEBUG("%s::%s: aServiceChild=%p", __CLASS__, __FUNCTION__,
                aServiceChild.get());

  mServiceChild = std::move(aServiceChild);

  nsTArray<MozPromiseHolder<GetServiceChildPromise>> holders =
      std::move(mGetServiceChildPromises);
  for (MozPromiseHolder<GetServiceChildPromise>& holder : holders) {
    holder.Resolve(mServiceChild.get(), __func__);
  }
}

void GeckoMediaPluginServiceChild::RemoveGMPContentParent(
    GMPContentParent* aGMPContentParent) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG(
      "%s::%s: aGMPContentParent=%p, mServiceChild=%p, "
      "mShuttingDownOnGMPThread=%s",
      __CLASS__, __FUNCTION__, aGMPContentParent, mServiceChild.get(),
      mShuttingDownOnGMPThread ? "true" : "false");

  if (mServiceChild) {
    mServiceChild->RemoveGMPContentParent(aGMPContentParent);
    GMP_LOG_DEBUG(
        "%s::%s: aGMPContentParent removed, "
        "mServiceChild->HaveContentParents()=%s",
        __CLASS__, __FUNCTION__,
        mServiceChild->HaveContentParents() ? "true" : "false");
    RemoveShutdownBlockerIfNeeded();
  }
}

nsresult GeckoMediaPluginServiceChild::AddShutdownBlocker() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDownOnGMPThread,
             "No call paths should add blockers once we're shutting down!");
  MOZ_ASSERT(!mShutdownBlockerAdded, "Should only add blocker once!");
  GMP_LOG_DEBUG("%s::%s ", __CLASS__, __FUNCTION__);

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  if (NS_WARN_IF(!barrier)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv =
      barrier->AddBlocker(this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
                          __LINE__, kShutdownBlockerName);
#ifdef DEBUG
  mShutdownBlockerAdded = NS_SUCCEEDED(rv);
#endif
  return rv;
}

void GeckoMediaPluginServiceChild::RemoveShutdownBlocker() {
  AssertOnGMPThread();
  MOZ_ASSERT(mShuttingDownOnGMPThread,
             "We should only remove blockers once we're "
             "shutting down!");
  GMP_LOG_DEBUG("%s::%s ", __CLASS__, __FUNCTION__);
  nsresult rv = mMainThread->Dispatch(NS_NewRunnableFunction(
      "GeckoMediaPluginServiceChild::"
      "RemoveShutdownBlocker",
      [this, self = RefPtr<GeckoMediaPluginServiceChild>(this)]() {
        nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
        if (NS_WARN_IF(!barrier)) {
          return;
        }

        nsresult rv = barrier->RemoveBlocker(this);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
      }));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ASSERT_UNREACHABLE(
        "Main thread should always be alive when we "
        "call this!");
  }
}

void GeckoMediaPluginServiceChild::RemoveShutdownBlockerIfNeeded() {
  AssertOnGMPThread();
  GMP_LOG_DEBUG(
      "%s::%s mPendingGetContentParents=%" PRIu32
      " mServiceChild->HaveContentParents()=%s "
      "mShuttingDownOnGMPThread=%s",
      __CLASS__, __FUNCTION__, mPendingGetContentParents,
      mServiceChild && mServiceChild->HaveContentParents() ? "true" : "false",
      mShuttingDownOnGMPThread ? "true" : "false");

  bool haveOneOrMoreContentParents =
      mPendingGetContentParents > 0 ||
      (mServiceChild && mServiceChild->HaveContentParents());

  if (!mShuttingDownOnGMPThread || haveOneOrMoreContentParents) {
    return;
  }
  RemoveShutdownBlocker();
}

already_AddRefed<GMPContentParent> GMPServiceChild::GetBridgedGMPContentParent(
    ProcessId aOtherPid, ipc::Endpoint<PGMPContentParent>&& endpoint) {
  return do_AddRef(mContentParents.LookupOrInsertWith(aOtherPid, [&] {
    MOZ_ASSERT(aOtherPid == endpoint.OtherPid());

    auto parent = MakeRefPtr<GMPContentParent>();

    DebugOnly<bool> ok = endpoint.Bind(parent);
    MOZ_ASSERT(ok);

    return parent;
  }));
}

void GMPServiceChild::RemoveGMPContentParent(
    GMPContentParent* aGMPContentParent) {
  for (auto iter = mContentParents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<GMPContentParent>& parent = iter.Data();
    if (parent == aGMPContentParent) {
      iter.Remove();
      break;
    }
  }
}

bool GMPServiceChild::HasAlreadyBridgedTo(base::ProcessId aPid) const {
  return mContentParents.Contains(aPid);
}

void GMPServiceChild::GetAndBlockAlreadyBridgedTo(
    nsTArray<base::ProcessId>& aAlreadyBridgedTo,
    nsTArray<RefPtr<GMPContentParentCloseBlocker>>& aBlockers) {
  aAlreadyBridgedTo.SetCapacity(mContentParents.Count());
  aBlockers.SetCapacity(mContentParents.Count());
  for (auto iter = mContentParents.Iter(); !iter.Done(); iter.Next()) {
    aAlreadyBridgedTo.AppendElement(iter.Key());
    aBlockers.AppendElement(new GMPContentParentCloseBlocker(iter.UserData()));
  }
}

class OpenPGMPServiceChild : public mozilla::Runnable {
 public:
  OpenPGMPServiceChild(RefPtr<GMPServiceChild>&& aGMPServiceChild,
                       ipc::Endpoint<PGMPServiceChild>&& aEndpoint)
      : Runnable("gmp::OpenPGMPServiceChild"),
        mGMPServiceChild(std::move(aGMPServiceChild)),
        mEndpoint(std::move(aEndpoint)) {}

  NS_IMETHOD Run() override {
    RefPtr<GeckoMediaPluginServiceChild> gmp =
        GeckoMediaPluginServiceChild::GetSingleton();
    MOZ_RELEASE_ASSERT(gmp);
    MOZ_ASSERT(!gmp->mServiceChild);
    if (mEndpoint.Bind(mGMPServiceChild.get())) {
      gmp->SetServiceChild(std::move(mGMPServiceChild));
    } else {
      gmp->SetServiceChild(nullptr);
    }
    return NS_OK;
  }

 private:
  RefPtr<GMPServiceChild> mGMPServiceChild;
  ipc::Endpoint<PGMPServiceChild> mEndpoint;
};

/* static */
bool GMPServiceChild::Create(Endpoint<PGMPServiceChild>&& aGMPService) {
  RefPtr<GeckoMediaPluginServiceChild> gmp =
      GeckoMediaPluginServiceChild::GetSingleton();
  if (NS_WARN_IF(!gmp)) {
    return false;
  }

  MOZ_ASSERT(!gmp->mServiceChild);

  RefPtr<GMPServiceChild> serviceChild(new GMPServiceChild());

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = gmp->GetThread(getter_AddRefs(gmpThread));
  NS_ENSURE_SUCCESS(rv, false);

  rv = gmpThread->Dispatch(
      new OpenPGMPServiceChild(std::move(serviceChild), std::move(aGMPService)),
      NS_DISPATCH_NORMAL);
  return NS_SUCCEEDED(rv);
}

ipc::IPCResult GMPServiceChild::RecvBeginShutdown() {
  RefPtr<GeckoMediaPluginServiceChild> service =
      GeckoMediaPluginServiceChild::GetSingleton();
  MOZ_ASSERT(service && service->mServiceChild.get() == this);
  if (service) {
    service->BeginShutdown();
  }
  return IPC_OK();
}

bool GMPServiceChild::HaveContentParents() const {
  return mContentParents.Count() > 0;
}

}  // namespace mozilla::gmp

#undef __CLASS__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceChild.h"

#include "GMPContentParent.h"
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
        UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>> holder(
            rawHolder);
        nsresult rv;

        nsTArray<base::ProcessId> alreadyBridgedTo;
        child->GetAlreadyBridgedTo(alreadyBridgedTo);

        base::ProcessId otherProcess;
        nsCString displayName;
        uint32_t pluginId = 0;
        ipc::Endpoint<PGMPContentParent> endpoint;
        nsCString errorDescription;

        bool ok = child->SendLaunchGMP(
            nodeIdVariant, api, tags, alreadyBridgedTo, &pluginId,
            &otherProcess, &displayName, &endpoint, &rv, &errorDescription);

        if (helper && pluginId) {
          // Note: Even if the launch failed, we need to connect the crash
          // helper so that if the launch failed due to the plugin crashing, we
          // can report the crash via the crash reporter. The crash handling
          // notification will arrive shortly if the launch failed due to the
          // plugin crashing.
          self->ConnectCrashHelper(pluginId, helper);
        }

        if (!ok || NS_FAILED(rv)) {
          MediaResult error(
              rv, nsPrintfCString(
                      "GeckoMediaPluginServiceChild::GetContentParent "
                      "SendLaunchGMPForNodeId failed with description (%s)",
                      errorDescription.get()));

          GMP_LOG_DEBUG("%s failed to launch GMP with error: %s", __CLASS__,
                        error.Description().get());
          self->mPendingGetContentParents -= 1;
          self->RemoveShutdownBlockerIfNeeded();

          holder->Reject(error, __func__);
          return;
        }

        RefPtr<GMPContentParent> parent = child->GetBridgedGMPContentParent(
            otherProcess, std::move(endpoint));
        if (!alreadyBridgedTo.Contains(otherProcess)) {
          parent->SetDisplayName(displayName);
          parent->SetPluginId(pluginId);
        }

        // The content parent is no longer pending.
        self->mPendingGetContentParents -= 1;
        MOZ_ASSERT(child->HaveContentParents(),
                   "We should have at least one content parent!");
        // We don't check if we need to remove the shutdown blocker here as
        // we should always have at least one live content parent.

        RefPtr<GMPContentParent::CloseBlocker> blocker(
            new GMPContentParent::CloseBlocker(parent));
        holder->Resolve(blocker, __func__);
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
                                              nsTArray<nsCString>* aTags,
                                              bool* aHasPlugin) {
  StaticMutexAutoLock lock(sGMPCapabilitiesMutex);
  if (!sGMPCapabilities) {
    *aHasPlugin = false;
    return NS_OK;
  }

  nsCString api(aAPI);
  for (const GMPCapabilityAndVersion& plugin : *sGMPCapabilities) {
    if (GMPCapability::Supports(plugin.mCapabilities, api, *aTags)) {
      *aHasPlugin = true;
      return NS_OK;
    }
  }

  *aHasPlugin = false;
  return NS_OK;
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
        UniquePtr<GetNodeIdCallback> callback(rawCallback);
        nsCString outId;
        if (!child->SendGetGMPNodeId(origin, topLevelOrigin, gmpName, &outId)) {
          callback->Done(NS_ERROR_FAILURE, ""_ns);
          return;
        }

        callback->Done(NS_OK, outId);
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
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
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
#ifdef DEBUG
  MOZ_ASSERT(!mShutdownBlockerAdded, "Should only add blocker once!");
  mShutdownBlockerAdded = true;
#endif
  GMP_LOG_DEBUG("%s::%s ", __CLASS__, __FUNCTION__);

  nsresult rv = GetShutdownBarrier()->AddBlocker(
      this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
      kShutdownBlockerName);
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
        nsresult rv = GetShutdownBarrier()->RemoveBlocker(this);
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

void GMPServiceChild::GetAlreadyBridgedTo(
    nsTArray<base::ProcessId>& aAlreadyBridgedTo) {
  AppendToArray(aAlreadyBridgedTo, mContentParents.Keys());
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceChild.h"

#include "base/task.h"
#include "GMPLog.h"
#include "GMPParent.h"
#include "GMPContentParent.h"
#include "mozIGeckoMediaPluginChromeService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsXPCOMPrivate.h"
#include "runnable_utils.h"

namespace mozilla {

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPServiceChild"

namespace gmp {

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

RefPtr<GetGMPContentParentPromise>
GeckoMediaPluginServiceChild::GetContentParent(
    GMPCrashHelper* aHelper, const nsACString& aNodeIdString,
    const nsCString& aAPI, const nsTArray<nsCString>& aTags) {
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  MozPromiseHolder<GetGMPContentParentPromise>* rawHolder =
      new MozPromiseHolder<GetGMPContentParentPromise>();
  RefPtr<GetGMPContentParentPromise> promise = rawHolder->Ensure(__func__);
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());

  nsCString nodeIdString(aNodeIdString);
  nsCString api(aAPI);
  RefPtr<GMPCrashHelper> helper(aHelper);
  RefPtr<GeckoMediaPluginServiceChild> self(this);
  GetServiceChild()->Then(
      thread, __func__,
      [self, nodeIdString, api, tags = aTags.Clone(), helper,
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
        nsCString errorDescription = NS_LITERAL_CSTRING("");

        bool ok = child->SendLaunchGMP(
            nodeIdString, api, tags, alreadyBridgedTo, &pluginId, &otherProcess,
            &displayName, &endpoint, &rv, &errorDescription);
        if (helper && pluginId) {
          // Note: Even if the launch failed, we need to connect the crash
          // helper so that if the launch failed due to the plugin crashing,
          // we can report the crash via the crash reporter. The crash
          // handling notification will arrive shortly if the launch failed
          // due to the plugin crashing.
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
          holder->Reject(error, __func__);
          return;
        }

        RefPtr<GMPContentParent> parent = child->GetBridgedGMPContentParent(
            otherProcess, std::move(endpoint));
        if (!alreadyBridgedTo.Contains(otherProcess)) {
          parent->SetDisplayName(displayName);
          parent->SetPluginId(pluginId);
        }
        RefPtr<GMPContentParent::CloseBlocker> blocker(
            new GMPContentParent::CloseBlocker(parent));
        holder->Resolve(blocker, __func__);
      },
      [rawHolder](MediaResult result) {
        UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>> holder(
            rawHolder);
        holder->Reject(result, __func__);
      });

  return promise;
}

RefPtr<GetGMPContentParentPromise>
GeckoMediaPluginServiceChild::GetContentParent(
    GMPCrashHelper* aHelper, const NodeId& aNodeId, const nsCString& aAPI,
    const nsTArray<nsCString>& aTags) {
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  MozPromiseHolder<GetGMPContentParentPromise>* rawHolder =
      new MozPromiseHolder<GetGMPContentParentPromise>();
  RefPtr<GetGMPContentParentPromise> promise = rawHolder->Ensure(__func__);
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());

  NodeIdData nodeId(aNodeId.mOrigin, aNodeId.mTopLevelOrigin, aNodeId.mGMPName);
  nsCString api(aAPI);
  RefPtr<GMPCrashHelper> helper(aHelper);
  RefPtr<GeckoMediaPluginServiceChild> self(this);
  GetServiceChild()->Then(
      thread, __func__,
      [self, nodeId, api, tags = aTags.Clone(), helper,
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
        nsCString errorDescription = NS_LITERAL_CSTRING("");

        bool ok = child->SendLaunchGMPForNodeId(
            nodeId, api, tags, alreadyBridgedTo, &pluginId, &otherProcess,
            &displayName, &endpoint, &rv, &errorDescription);

        if (helper && pluginId) {
          // Note: Even if the launch failed, we need to connect the crash
          // helper so that if the launch failed due to the plugin crashing,
          // we can report the crash via the crash reporter. The crash
          // handling notification will arrive shortly if the launch failed
          // due to the plugin crashing.
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
          holder->Reject(error, __func__);
          return;
        }

        RefPtr<GMPContentParent> parent = child->GetBridgedGMPContentParent(
            otherProcess, std::move(endpoint));
        if (!alreadyBridgedTo.Contains(otherProcess)) {
          parent->SetDisplayName(displayName);
          parent->SetPluginId(pluginId);
        }

        RefPtr<GMPContentParent::CloseBlocker> blocker(
            new GMPContentParent::CloseBlocker(parent));
        holder->Resolve(blocker, __func__);
      },
      [rawHolder](MediaResult result) {
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
      for (const nsCString& tag : tags.tags()) {
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
    nsCString tags;
    for (const GMPCapability& cap : mCapabilities) {
      if (!tags.IsEmpty()) {
        tags.AppendLiteral(" ");
      }
      tags.Append(cap.mAPIName);
      for (const nsCString& tag : cap.mAPITags) {
        tags.AppendLiteral(":");
        tags.Append(tag);
      }
    }
    s.Append(tags);
    s.AppendLiteral("]");
    return s;
  }

  nsCString mName;
  nsCString mVersion;
  nsTArray<GMPCapability> mCapabilities;
};

StaticMutex sGMPCapabilitiesMutex;
StaticAutoPtr<nsTArray<GMPCapabilityAndVersion>> sGMPCapabilities;

static nsCString GMPCapabilitiesToString() {
  nsCString s;
  for (const GMPCapabilityAndVersion& gmp : *sGMPCapabilities) {
    if (!s.IsEmpty()) {
      s.AppendLiteral(", ");
    }
    s.Append(gmp.ToString());
  }
  return s;
}

/* static */
void GeckoMediaPluginServiceChild::UpdateGMPCapabilities(
    nsTArray<GMPCapabilityData>&& aCapabilities) {
  {
    // The mutex should unlock before sending the "gmp-changed" observer service
    // notification.
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
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  mShuttingDownOnGMPThread = true;
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
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  GetNodeIdCallback* rawCallback = aCallback.release();
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  nsString origin(aOrigin);
  nsString topLevelOrigin(aTopLevelOrigin);
  nsString gmpName(aGMPName);
  GetServiceChild()->Then(
      thread, __func__,
      [rawCallback, origin, topLevelOrigin, gmpName](GMPServiceChild* child) {
        UniquePtr<GetNodeIdCallback> callback(rawCallback);
        nsCString outId;
        if (!child->SendGetGMPNodeId(origin, topLevelOrigin, gmpName, &outId)) {
          callback->Done(NS_ERROR_FAILURE, EmptyCString());
          return;
        }

        callback->Done(NS_OK, outId);
      },
      [rawCallback](nsresult rv) {
        UniquePtr<GetNodeIdCallback> callback(rawCallback);
        callback->Done(NS_ERROR_FAILURE, EmptyCString());
      });

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aSomeData) {
  GMP_LOG_DEBUG("%s::%s: %s", __CLASS__, __FUNCTION__, aTopic);
  if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    if (mServiceChild) {
      mozilla::SyncRunnable::DispatchToThread(
          mGMPThread,
          WrapRunnable(mServiceChild.get(), &PGMPServiceChild::Close));
      mServiceChild = nullptr;
    }
    ShutdownGMPThread();
  } else if (!strcmp(NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, aTopic)) {
    mXPCOMWillShutdown = true;
  }

  return NS_OK;
}

RefPtr<GeckoMediaPluginServiceChild::GetServiceChildPromise>
GeckoMediaPluginServiceChild::GetServiceChild() {
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

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
    UniquePtr<GMPServiceChild>&& aServiceChild) {
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  mServiceChild = std::move(aServiceChild);

  nsTArray<MozPromiseHolder<GetServiceChildPromise>> holders;
  holders.SwapElements(mGetServiceChildPromises);
  for (MozPromiseHolder<GetServiceChildPromise>& holder : holders) {
    holder.Resolve(mServiceChild.get(), __func__);
  }
}

void GeckoMediaPluginServiceChild::RemoveGMPContentParent(
    GMPContentParent* aGMPContentParent) {
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  if (mServiceChild) {
    mServiceChild->RemoveGMPContentParent(aGMPContentParent);
    if (mShuttingDownOnGMPThread && !mServiceChild->HaveContentParents()) {
      mServiceChild->Close();
      mServiceChild = nullptr;
    }
  }
}

GMPServiceChild::GMPServiceChild() = default;

GMPServiceChild::~GMPServiceChild() = default;

already_AddRefed<GMPContentParent> GMPServiceChild::GetBridgedGMPContentParent(
    ProcessId aOtherPid, ipc::Endpoint<PGMPContentParent>&& endpoint) {
  RefPtr<GMPContentParent> parent;
  mContentParents.Get(aOtherPid, getter_AddRefs(parent));

  if (parent) {
    return parent.forget();
  }

  MOZ_ASSERT(aOtherPid == endpoint.OtherPid());

  parent = new GMPContentParent();

  DebugOnly<bool> ok = endpoint.Bind(parent);
  MOZ_ASSERT(ok);

  mContentParents.Put(aOtherPid, RefPtr{parent});

  return parent.forget();
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
  aAlreadyBridgedTo.SetCapacity(mContentParents.Count());
  for (auto iter = mContentParents.Iter(); !iter.Done(); iter.Next()) {
    const uint64_t& id = iter.Key();
    aAlreadyBridgedTo.AppendElement(id);
  }
}

class OpenPGMPServiceChild : public mozilla::Runnable {
 public:
  OpenPGMPServiceChild(UniquePtr<GMPServiceChild>&& aGMPServiceChild,
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
  UniquePtr<GMPServiceChild> mGMPServiceChild;
  ipc::Endpoint<PGMPServiceChild> mEndpoint;
};

/* static */
bool GMPServiceChild::Create(Endpoint<PGMPServiceChild>&& aGMPService) {
  RefPtr<GeckoMediaPluginServiceChild> gmp =
      GeckoMediaPluginServiceChild::GetSingleton();
  MOZ_ASSERT(!gmp->mServiceChild);

  UniquePtr<GMPServiceChild> serviceChild(new GMPServiceChild());

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

}  // namespace gmp
}  // namespace mozilla

#undef __CLASS__

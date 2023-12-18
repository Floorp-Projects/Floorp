/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceParent.h"

#include <limits>

#include "GMPDecoderModule.h"
#include "GMPLog.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "base/task.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsThreadUtils.h"
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxInfo.h"
#endif
#include "VideoUtils.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#if defined(XP_WIN)
#  include "mozilla/UntrustedModulesData.h"
#endif
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIXULRuntime.h"
#include "nsNativeCharsetUtils.h"
#include "nsXPCOMPrivate.h"
#include "prio.h"
#include "runnable_utils.h"

#ifdef DEBUG
#  include "mozilla/dom/MediaKeys.h"  // MediaKeys::kMediaKeysRequestTopic
#endif

#ifdef MOZ_WMF_CDM
#  include "mozilla/MFCDMParent.h"
#endif

namespace mozilla::gmp {

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPServiceParent"

static const uint32_t NodeIdSaltLength = 32;

already_AddRefed<GeckoMediaPluginServiceParent>
GeckoMediaPluginServiceParent::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<GeckoMediaPluginService> service(
      GeckoMediaPluginServiceParent::GetGeckoMediaPluginService());
#ifdef DEBUG
  if (service) {
    nsCOMPtr<mozIGeckoMediaPluginChromeService> chromeService;
    CallQueryInterface(service.get(), getter_AddRefs(chromeService));
    MOZ_ASSERT(chromeService);
  }
#endif
  return service.forget().downcast<GeckoMediaPluginServiceParent>();
}

NS_IMPL_ISUPPORTS_INHERITED(GeckoMediaPluginServiceParent,
                            GeckoMediaPluginService,
                            mozIGeckoMediaPluginChromeService,
                            nsIAsyncShutdownBlocker)

GeckoMediaPluginServiceParent::GeckoMediaPluginServiceParent()
    : mScannedPluginOnDisk(false),
      mShuttingDown(false),
      mWaitingForPluginsSyncShutdown(false),
      mInitPromiseMonitor("GeckoMediaPluginServiceParent::mInitPromiseMonitor"),
      mInitPromise(&mInitPromiseMonitor),
      mLoadPluginsFromDiskComplete(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

GeckoMediaPluginServiceParent::~GeckoMediaPluginServiceParent() {
  MOZ_ASSERT(mPlugins.IsEmpty());
}

nsresult GeckoMediaPluginServiceParent::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  if (AppShutdown::GetCurrentShutdownPhase() != ShutdownPhase::NotInShutdown) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);

  MOZ_ALWAYS_SUCCEEDS(
      obsService->AddObserver(this, "profile-change-teardown", false));
  MOZ_ALWAYS_SUCCEEDS(
      obsService->AddObserver(this, "last-pb-context-exited", false));
  MOZ_ALWAYS_SUCCEEDS(
      obsService->AddObserver(this, "browser:purge-session-history", false));
  MOZ_ALWAYS_SUCCEEDS(
      obsService->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false));
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, "nsPref:changed", false));

#ifdef DEBUG
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(
      this, dom::MediaKeys::kMediaKeysRequestTopic, false));
#endif

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->AddObserver("media.gmp.plugin.crash", this, false);
  }

  nsresult rv = InitStorage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Kick off scanning for plugins
  nsCOMPtr<nsIThread> thread;
  rv = GetThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Detect if GMP storage has an incompatible version, and if so nuke it.
  int32_t version =
      Preferences::GetInt("media.gmp.storage.version.observed", 0);
  int32_t expected =
      Preferences::GetInt("media.gmp.storage.version.expected", 0);
  if (version != expected) {
    Preferences::SetInt("media.gmp.storage.version.observed", expected);
    return GMPDispatch(
        NewRunnableMethod("gmp::GeckoMediaPluginServiceParent::ClearStorage",
                          this, &GeckoMediaPluginServiceParent::ClearStorage));
  }
  return NS_OK;
}

already_AddRefed<nsIFile> CloneAndAppend(nsIFile* aFile,
                                         const nsAString& aDir) {
  nsCOMPtr<nsIFile> f;
  nsresult rv = aFile->Clone(getter_AddRefs(f));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = f->Append(aDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return f.forget();
}

static nsresult GMPPlatformString(nsAString& aOutPlatform) {
  // Append the OS and arch so that we don't reuse the storage if the profile is
  // copied or used under a different bit-ness, or copied to another platform.
  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString OS;
  nsresult rv = runtime->GetOS(OS);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString arch;
  rv = runtime->GetXPCOMABI(arch);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString platform;
  platform.Append(OS);
  platform.AppendLiteral("_");
  platform.Append(arch);

  CopyUTF8toUTF16(platform, aOutPlatform);

  return NS_OK;
}

nsresult GeckoMediaPluginServiceParent::InitStorage() {
  MOZ_ASSERT(NS_IsMainThread());

  // GMP storage should be used in the chrome process only.
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  // Directory service is main thread only, so cache the profile dir here
  // so that we can use it off main thread.
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mStorageBaseDir));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mStorageBaseDir->AppendNative("gmp"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString platform;
  rv = GMPPlatformString(platform);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mStorageBaseDir->Append(platform);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return GeckoMediaPluginService::Init();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aSomeData) {
  GMP_LOG_DEBUG("%s::%s topic='%s' data='%s'", __CLASS__, __FUNCTION__, aTopic,
                NS_ConvertUTF16toUTF8(aSomeData).get());
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(aSubject));
    if (branch) {
      bool crashNow = false;
      if (u"media.gmp.plugin.crash"_ns.Equals(aSomeData)) {
        branch->GetBoolPref("media.gmp.plugin.crash", &crashNow);
      }
      if (crashNow) {
        nsCOMPtr<nsIThread> gmpThread;
        {
          MutexAutoLock lock(mMutex);
          gmpThread = mGMPThread;
        }
        if (gmpThread) {
          // Note: the GeckoMediaPluginServiceParent singleton is kept alive by
          // a static refptr that is only cleared in the final stage of shutdown
          // after everything else is shutdown, so this RefPtr<> is not strictly
          // necessary so long as that is true, but it's safer.
          gmpThread->Dispatch(
              WrapRunnable(RefPtr<GeckoMediaPluginServiceParent>(this),
                           &GeckoMediaPluginServiceParent::CrashPlugins),
              NS_DISPATCH_NORMAL);
        }
      }
    }
  } else if (!strcmp("profile-change-teardown", aTopic)) {
    mWaitingForPluginsSyncShutdown = true;

    nsCOMPtr<nsIThread> gmpThread;
    DebugOnly<bool> plugins_empty;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mShuttingDown);
      mShuttingDown = true;
      gmpThread = mGMPThread;
#ifdef DEBUG
      plugins_empty = mPlugins.IsEmpty();
#endif
    }

    if (gmpThread) {
      GMP_LOG_DEBUG(
          "%s::%s Starting to unload plugins, waiting for sync shutdown...",
          __CLASS__, __FUNCTION__);
      gmpThread->Dispatch(
          NewRunnableMethod("gmp::GeckoMediaPluginServiceParent::UnloadPlugins",
                            this,
                            &GeckoMediaPluginServiceParent::UnloadPlugins),
          NS_DISPATCH_NORMAL);

      // Wait for UnloadPlugins() to do sync shutdown...
      SpinEventLoopUntil(
          "GeckoMediaPluginServiceParent::Observe "
          "WaitingForPluginsSyncShutdown"_ns,
          [&]() { return !mWaitingForPluginsSyncShutdown; });
    } else {
      // GMP thread has already shutdown.
      MOZ_ASSERT(plugins_empty);
      mWaitingForPluginsSyncShutdown = false;
    }

  } else if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
#ifdef DEBUG
    MOZ_ASSERT(mShuttingDown);
#endif
    ShutdownGMPThread();
  } else if (!strcmp(NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, aTopic)) {
    mXPCOMWillShutdown = true;
  } else if (!strcmp("last-pb-context-exited", aTopic)) {
    // When Private Browsing mode exits, all we need to do is clear
    // mTempNodeIds. This drops all the node ids we've cached in memory
    // for PB origin-pairs. If we try to open an origin-pair for non-PB
    // mode, we'll get the NodeId salt stored on-disk, and if we try to
    // open a PB mode origin-pair, we'll re-generate new salt.
    mTempNodeIds.Clear();
  } else if (!strcmp("browser:purge-session-history", aTopic)) {
    // Clear everything!
    if (!aSomeData || nsDependentString(aSomeData).IsEmpty()) {
      return GMPDispatch(NewRunnableMethod(
          "gmp::GeckoMediaPluginServiceParent::ClearStorage", this,
          &GeckoMediaPluginServiceParent::ClearStorage));
    }

    // Clear nodeIds/records modified after |t|.
    nsresult rv;
    PRTime t = nsDependentString(aSomeData).ToInteger64(&rv, 10);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return GMPDispatch(NewRunnableMethod<PRTime>(
        "gmp::GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread",
        this, &GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread,
        t));
  } else if (!strcmp("nsPref:changed", aTopic)) {
    bool hasProcesses = false;
    {
      MutexAutoLock lock(mMutex);
      for (const auto& plugin : mPlugins) {
        if (plugin->State() == GMPState::Loaded) {
          hasProcesses = true;
          break;
        }
      }
    }

    if (hasProcesses) {
      // We know prefs are ASCII here.
      NS_LossyConvertUTF16toASCII strData(aSomeData);
      mozilla::dom::Pref pref(strData, /* isLocked */ false,
                              /* isSanitized */ false, Nothing(), Nothing());
      Preferences::GetPreference(&pref, GeckoProcessType_GMPlugin,
                                 /* remoteType */ ""_ns);
      return GMPDispatch(NewRunnableMethod<mozilla::dom::Pref&&>(
          "gmp::GeckoMediaPluginServiceParent::OnPreferenceChanged", this,
          &GeckoMediaPluginServiceParent::OnPreferenceChanged,
          std::move(pref)));
    }
  }

  return NS_OK;
}

void GeckoMediaPluginServiceParent::OnPreferenceChanged(
    mozilla::dom::Pref&& aPref) {
  AssertOnGMPThread();

  MutexAutoLock lock(mMutex);
  for (const auto& plugin : mPlugins) {
    plugin->OnPreferenceChange(aPref);
  }
}

RefPtr<GenericPromise> GeckoMediaPluginServiceParent::EnsureInitialized() {
  MonitorAutoLock lock(mInitPromiseMonitor);
  if (mLoadPluginsFromDiskComplete) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }
  // We should have an init promise in flight.
  MOZ_ASSERT(!mInitPromise.IsEmpty());
  return mInitPromise.Ensure(__func__);
}

RefPtr<GetGMPContentParentPromise>
GeckoMediaPluginServiceParent::GetContentParent(
    GMPCrashHelper* aHelper, const NodeIdVariant& aNodeIdVariant,
    const nsACString& aAPI, const nsTArray<nsCString>& aTags) {
  AssertOnGMPThread();

  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());
  if (!thread) {
    MOZ_ASSERT_UNREACHABLE(
        "We should always be called on GMP thread, so it should be live");
    return GetGMPContentParentPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  nsCString nodeIdString;
  nsresult rv = GetNodeId(aNodeIdVariant, nodeIdString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GetGMPContentParentPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  auto holder = MakeUnique<MozPromiseHolder<GetGMPContentParentPromise>>();
  RefPtr<GetGMPContentParentPromise> promise = holder->Ensure(__func__);
  EnsureInitialized()->Then(
      thread, __func__,
      [self = RefPtr<GeckoMediaPluginServiceParent>(this),
       nodeIdString = std::move(nodeIdString), api = nsCString(aAPI),
       tags = aTags.Clone(), helper = RefPtr<GMPCrashHelper>(aHelper),
       holder = std::move(holder)](
          const GenericPromise::ResolveOrRejectValue& aValue) mutable -> void {
        if (aValue.IsReject()) {
          NS_WARNING("GMPService::EnsureInitialized failed.");
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        RefPtr<GMPParent> gmp =
            self->SelectPluginForAPI(nodeIdString, api, tags);
        GMP_LOG_DEBUG("%s: %p returning %p for api %s", __FUNCTION__,
                      self.get(), gmp.get(), api.get());
        if (!gmp) {
          NS_WARNING(
              "GeckoMediaPluginServiceParent::GetContentParentFrom failed");
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        self->ConnectCrashHelper(gmp->GetPluginId(), helper);
        gmp->GetGMPContentParent(std::move(holder));
      });

  return promise;
}

void GeckoMediaPluginServiceParent::InitializePlugins(
    nsISerialEventTarget* aGMPThread) {
  MOZ_ASSERT(aGMPThread);
  MonitorAutoLock lock(mInitPromiseMonitor);
  if (mLoadPluginsFromDiskComplete) {
    return;
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  RefPtr<GenericPromise> p = mInitPromise.Ensure(__func__);
  InvokeAsync(aGMPThread, this, __func__,
              &GeckoMediaPluginServiceParent::LoadFromEnvironment)
      ->Then(
          aGMPThread, __func__,
          [self]() -> void {
            MonitorAutoLock lock(self->mInitPromiseMonitor);
            self->mLoadPluginsFromDiskComplete = true;
            self->mInitPromise.Resolve(true, __func__);
          },
          [self]() -> void {
            MonitorAutoLock lock(self->mInitPromiseMonitor);
            self->mLoadPluginsFromDiskComplete = true;
            self->mInitPromise.Reject(NS_ERROR_FAILURE, __func__);
          });
}

void GeckoMediaPluginServiceParent::NotifySyncShutdownComplete() {
  MOZ_ASSERT(NS_IsMainThread());
  mWaitingForPluginsSyncShutdown = false;
}

bool GeckoMediaPluginServiceParent::IsShuttingDown() {
  AssertOnGMPThread();
  return mShuttingDownOnGMPThread;
}

void GeckoMediaPluginServiceParent::UnloadPlugins() {
  AssertOnGMPThread();
  MOZ_ASSERT(!mShuttingDownOnGMPThread);
  mShuttingDownOnGMPThread = true;

  nsTArray<RefPtr<GMPParent>> plugins;
  {
    MutexAutoLock lock(mMutex);
    // Move all plugins references to a local array. This way mMutex won't be
    // locked when calling CloseActive (to avoid inter-locking).
    std::swap(plugins, mPlugins);

    for (GMPServiceParent* parent : mServiceParents) {
      Unused << parent->SendBeginShutdown();
    }

    GMP_LOG_DEBUG("%s::%s plugins:%zu", __CLASS__, __FUNCTION__,
                  plugins.Length());
#ifdef DEBUG
    for (const auto& plugin : plugins) {
      GMP_LOG_DEBUG("%s::%s plugin: '%s'", __CLASS__, __FUNCTION__,
                    plugin->GetDisplayName().get());
    }
#endif
  }

  // Note: CloseActive may be async; it could actually finish
  // shutting down when all the plugins have unloaded.
  for (const auto& plugin : plugins) {
    plugin->CloseActive(true);
  }

  nsCOMPtr<nsIRunnable> task = NewRunnableMethod(
      "GeckoMediaPluginServiceParent::NotifySyncShutdownComplete", this,
      &GeckoMediaPluginServiceParent::NotifySyncShutdownComplete);
  mMainThread->Dispatch(task.forget());
}

void GeckoMediaPluginServiceParent::CrashPlugins() {
  GMP_LOG_DEBUG("%s::%s", __CLASS__, __FUNCTION__);
  AssertOnGMPThread();

  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->Crash();
  }
}

RefPtr<GenericPromise> GeckoMediaPluginServiceParent::LoadFromEnvironment() {
  AssertOnGMPThread();
  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());
  if (!thread) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  const char* env = PR_GetEnv("MOZ_GMP_PATH");
  if (!env || !*env) {
    return GenericPromise::CreateAndResolve(true, __func__);
  }

  nsString allpaths;
  if (NS_WARN_IF(NS_FAILED(
          NS_CopyNativeToUnicode(nsDependentCString(env), allpaths)))) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsTArray<RefPtr<GenericPromise>> promises;
  uint32_t pos = 0;
  while (pos < allpaths.Length()) {
    // Loop over multiple path entries separated by colons (*nix) or
    // semicolons (Windows)
    int32_t next = allpaths.FindChar(XPCOM_ENV_PATH_SEPARATOR[0], pos);
    if (next == -1) {
      promises.AppendElement(
          AddOnGMPThread(nsString(Substring(allpaths, pos))));
      break;
    } else {
      promises.AppendElement(
          AddOnGMPThread(nsString(Substring(allpaths, pos, next - pos))));
      pos = next + 1;
    }
  }

  mScannedPluginOnDisk = true;
  return GenericPromise::All(thread, promises)
      ->Then(
          thread, __func__,
          []() { return GenericPromise::CreateAndResolve(true, __func__); },
          []() {
            return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
          });
}

class NotifyObserversTask final : public mozilla::Runnable {
 public:
  explicit NotifyObserversTask(const char* aTopic, nsString aData = u""_ns)
      : Runnable(aTopic), mTopic(aTopic), mData(aData) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(obsService);
    if (obsService) {
      obsService->NotifyObservers(nullptr, mTopic, mData.get());
    }
    return NS_OK;
  }

 private:
  ~NotifyObserversTask() = default;
  const char* mTopic;
  const nsString mData;
};

NS_IMETHODIMP
GeckoMediaPluginServiceParent::PathRunnable::Run() {
  mService->RemoveOnGMPThread(mPath, mOperation == REMOVE_AND_DELETE_FROM_DISK,
                              mDefer);

  mService->UpdateContentProcessGMPCapabilities();
  return NS_OK;
}

void GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities(
    ContentParent* aContentProcess) {
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task = NewRunnableMethod<ContentParent*>(
        "GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities",
        this,
        &GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities,
        aContentProcess);
    mMainThread->Dispatch(task.forget());
    return;
  }

  typedef mozilla::dom::GMPCapabilityData GMPCapabilityData;
  typedef mozilla::dom::GMPAPITags GMPAPITags;
  typedef mozilla::dom::ContentParent ContentParent;

  const uint32_t NO_H264 = 0;
  const uint32_t HAS_H264 = 1;
  const uint32_t NO_H264_1_DIR = 2;
  const uint32_t NO_H264_2_PLUS_DIRS = 3;
  const uint32_t NO_H264_DIR_IN_PROGRESS = 4;
  uint32_t hasH264 = NO_H264;
  if (mDirectoriesAdded == 1) {
    hasH264 = NO_H264_1_DIR;
  } else if (mDirectoriesAdded > 1) {
    hasH264 = NO_H264_2_PLUS_DIRS;
  }
  if (mDirectoriesInProgress) {
    hasH264 = NO_H264_DIR_IN_PROGRESS;
  }
  nsTArray<GMPCapabilityData> caps;
  {
    MutexAutoLock lock(mMutex);
    for (const RefPtr<GMPParent>& gmp : mPlugins) {
      // We have multiple instances of a GMPParent for a given GMP in the
      // list, one per origin. So filter the list so that we don't include
      // the same GMP's capabilities twice.
      NS_ConvertUTF16toUTF8 name(gmp->GetPluginBaseName());
      bool found = false;
      for (const GMPCapabilityData& cap : caps) {
        if (cap.name().Equals(name)) {
          found = true;
          break;
        }
      }
      if (found) {
        continue;
      }
      GMPCapabilityData x;
      x.name() = name;
      x.version() = gmp->GetVersion();
      for (const GMPCapability& tag : gmp->GetCapabilities()) {
        x.capabilities().AppendElement(GMPAPITags(tag.mAPIName, tag.mAPITags));
        if (tag.mAPIName == nsLiteralCString(GMP_API_VIDEO_ENCODER) &&
            tag.mAPITags.Contains("h264"_ns)) {
          hasH264 = HAS_H264;
        }
      }
#ifdef MOZ_WMF_CDM
      if (name.Equals("gmp-widevinecdm-l1")) {
        nsCOMPtr<nsIFile> pluginFile = gmp->GetDirectory();
        MFCDMService::UpdateWidevineL1Path(pluginFile);
      }
#endif
      caps.AppendElement(std::move(x));
    }
  }

  Telemetry::Accumulate(Telemetry::MEDIA_GMP_UPDATE_CONTENT_PROCESS_HAS_H264,
                        hasH264);

  if (aContentProcess) {
    Unused << aContentProcess->SendGMPsChanged(caps);
    return;
  }

  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendGMPsChanged(caps);
  }

  // For non-e10s, we must fire a notification so that any MediaKeySystemAccess
  // requests waiting on a CDM to download will retry.
  nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  if (obsService) {
    obsService->NotifyObservers(nullptr, "gmp-changed", nullptr);
  }
}

void GeckoMediaPluginServiceParent::SendFlushFOGData(
    nsTArray<RefPtr<FlushFOGDataPromise>>& promises) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  for (const RefPtr<GMPParent>& gmp : mPlugins) {
    if (gmp->State() != GMPState::Loaded) {
      // Plugins that are not in the Loaded state have no process attached to
      // them, and any IPC we would attempt to send them would be ignored (or
      // result in a warning on debug builds).
      continue;
    }
    RefPtr<FlushFOGDataPromise::Private> promise =
        new FlushFOGDataPromise::Private(__func__);
    // Direct dispatch will resolve the promise on the same thread, which is
    // faster; FOGIPC will move execution back to the main thread.
    promise->UseDirectTaskDispatch(__func__);
    promises.EmplaceBack(promise);

    mGMPThread->Dispatch(
        NewRunnableMethod<ipc::ResolveCallback<ipc::ByteBuf>&&,
                          ipc::RejectCallback&&>(
            "GMPParent::SendFlushFOGData", gmp,
            static_cast<void (GMPParent::*)(
                mozilla::ipc::ResolveCallback<ipc::ByteBuf>&& aResolve,
                mozilla::ipc::RejectCallback&& aReject)>(
                &GMPParent::SendFlushFOGData),

            [promise](ipc::ByteBuf&& aValue) {
              promise->Resolve(std::move(aValue), __func__);
            },
            [promise](ipc::ResponseRejectReason&& aReason) {
              promise->Reject(std::move(aReason), __func__);
            }),
        NS_DISPATCH_NORMAL);
  }
}

#if defined(XP_WIN)
void GeckoMediaPluginServiceParent::SendGetUntrustedModulesData(
    nsTArray<RefPtr<GetUntrustedModulesDataPromise>>& promises) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  for (const RefPtr<GMPParent>& gmp : mPlugins) {
    if (gmp->State() != GMPState::Loaded) {
      // Plugins that are not in the Loaded state have no process attached to
      // them, and any IPC we would attempt to send them would be ignored (or
      // result in a warning on debug builds).
      continue;
    }
    RefPtr<GetUntrustedModulesDataPromise::Private> promise =
        new GetUntrustedModulesDataPromise::Private(__func__);
    // Direct dispatch will resolve the promise on the same thread, which is
    // faster; IPC will move execution back to the main thread.
    promise->UseDirectTaskDispatch(__func__);
    promises.EmplaceBack(promise);

    mGMPThread->Dispatch(
        NewRunnableMethod<ipc::ResolveCallback<Maybe<UntrustedModulesData>>&&,
                          ipc::RejectCallback&&>(
            "GMPParent::SendGetUntrustedModulesData", gmp,
            static_cast<void (GMPParent::*)(
                mozilla::ipc::ResolveCallback<Maybe<UntrustedModulesData>>&&
                    aResolve,
                mozilla::ipc::RejectCallback&& aReject)>(
                &GMPParent::SendGetUntrustedModulesData),

            [promise](Maybe<UntrustedModulesData>&& aValue) {
              promise->Resolve(std::move(aValue), __func__);
            },
            [promise](ipc::ResponseRejectReason&& aReason) {
              promise->Reject(std::move(aReason), __func__);
            }),
        NS_DISPATCH_NORMAL);
  }
}

void GeckoMediaPluginServiceParent::SendUnblockUntrustedModulesThread() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);

  for (const RefPtr<GMPParent>& gmp : mPlugins) {
    if (gmp->State() != GMPState::Loaded) {
      // Plugins that are not in the Loaded state have no process attached to
      // them, and any IPC we would attempt to send them would be ignored (or
      // result in a warning on debug builds).
      continue;
    }

    mGMPThread->Dispatch(
        NewRunnableMethod<>("GMPParent::SendUnblockUntrustedModulesThread", gmp,
                            static_cast<bool (GMPParent::*)()>(
                                &GMPParent::SendUnblockUntrustedModulesThread)),
        NS_DISPATCH_NORMAL);
  }
}
#endif

RefPtr<PGMPParent::TestTriggerMetricsPromise>
GeckoMediaPluginServiceParent::TestTriggerMetrics() {
  MOZ_ASSERT(NS_IsMainThread());
  {
    MutexAutoLock lock(mMutex);
    for (const RefPtr<GMPParent>& gmp : mPlugins) {
      if (gmp->State() != GMPState::Loaded) {
        // Plugins that are not in the Loaded state have no process attached to
        // them, and any IPC we would attempt to send them would be ignored (or
        // result in a warning on debug builds).
        continue;
      }

      RefPtr<PGMPParent::TestTriggerMetricsPromise::Private> promise =
          new PGMPParent::TestTriggerMetricsPromise::Private(__func__);
      // Direct dispatch will resolve the promise on the same thread, which is
      // faster; FOGIPC will move execution back to the main thread.
      promise->UseDirectTaskDispatch(__func__);

      mGMPThread->Dispatch(
          NewRunnableMethod<ipc::ResolveCallback<bool>&&,
                            ipc::RejectCallback&&>(
              "GMPParent::SendTestTriggerMetrics", gmp,
              static_cast<void (GMPParent::*)(
                  mozilla::ipc::ResolveCallback<bool>&& aResolve,
                  mozilla::ipc::RejectCallback&& aReject)>(
                  &PGMPParent::SendTestTriggerMetrics),

              [promise](bool aValue) {
                promise->Resolve(std::move(aValue), __func__);
              },
              [promise](ipc::ResponseRejectReason&& aReason) {
                promise->Reject(std::move(aReason), __func__);
              }),
          NS_DISPATCH_NORMAL);

      return promise;
    }
  }

  return PGMPParent::TestTriggerMetricsPromise::CreateAndReject(
      ipc::ResponseRejectReason::SendError, __func__);
}

RefPtr<GenericPromise> GeckoMediaPluginServiceParent::AsyncAddPluginDirectory(
    const nsAString& aDirectory) {
  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());
  if (!thread) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mDirectoriesAdded++;
  mDirectoriesInProgress++;

  nsString dir(aDirectory);
  RefPtr<GeckoMediaPluginServiceParent> self = this;
  return InvokeAsync(thread, this, __func__,
                     &GeckoMediaPluginServiceParent::AddOnGMPThread, dir)
      ->Then(
          mMainThread, __func__,
          [dir, self](bool aVal) {
            GMP_LOG_DEBUG(
                "GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s "
                "succeeded",
                NS_ConvertUTF16toUTF8(dir).get());
            MOZ_ASSERT(NS_IsMainThread());
            self->mDirectoriesInProgress--;
            self->UpdateContentProcessGMPCapabilities();
            return GenericPromise::CreateAndResolve(aVal, __func__);
          },
          [dir, self](nsresult aResult) {
            GMP_LOG_DEBUG(
                "GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s "
                "failed",
                NS_ConvertUTF16toUTF8(dir).get());
            self->mDirectoriesInProgress--;
            return GenericPromise::CreateAndReject(aResult, __func__);
          });
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::AddPluginDirectory(const nsAString& aDirectory) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<GenericPromise> p = AsyncAddPluginDirectory(aDirectory);
  Unused << p;
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::RemovePluginDirectory(
    const nsAString& aDirectory) {
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(
      new PathRunnable(this, aDirectory, PathRunnable::EOperation::REMOVE));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::RemoveAndDeletePluginDirectory(
    const nsAString& aDirectory, const bool aDefer) {
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(new PathRunnable(
      this, aDirectory, PathRunnable::EOperation::REMOVE_AND_DELETE_FROM_DISK,
      aDefer));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::HasPluginForAPI(const nsACString& aAPI,
                                               const nsTArray<nsCString>& aTags,
                                               bool* aHasPlugin) {
  NS_ENSURE_ARG(!aTags.IsEmpty());
  NS_ENSURE_ARG(aHasPlugin);

  nsresult rv = EnsurePluginsOnDiskScanned();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load GMPs from disk.");
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    nsCString api(aAPI);
    size_t index = 0;
    RefPtr<GMPParent> gmp = FindPluginForAPIFrom(index, api, aTags, &index);
    *aHasPlugin = !!gmp;
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::FindPluginDirectoryForAPI(
    const nsACString& aAPI, const nsTArray<nsCString>& aTags,
    nsIFile** aDirectory) {
  NS_ENSURE_ARG(!aTags.IsEmpty());
  NS_ENSURE_ARG(aDirectory);

  nsresult rv = EnsurePluginsOnDiskScanned();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load GMPs from disk.");
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    nsCString api(aAPI);
    size_t index = 0;
    RefPtr<GMPParent> gmp = FindPluginForAPIFrom(index, api, aTags, &index);
    if (gmp) {
      nsCOMPtr<nsIFile> dir = gmp->GetDirectory();
      dir.forget(aDirectory);
    }
  }

  return NS_OK;
}

nsresult GeckoMediaPluginServiceParent::EnsurePluginsOnDiskScanned() {
  const char* env = nullptr;
  if (!mScannedPluginOnDisk && (env = PR_GetEnv("MOZ_GMP_PATH")) && *env) {
    // We have a MOZ_GMP_PATH environment variable which may specify the
    // location of plugins to load, and we haven't yet scanned the disk to
    // see if there are plugins there. Get the GMP thread, which will
    // cause an event to be dispatched to which scans for plugins. We
    // dispatch a sync event to the GMP thread here in order to wait until
    // after the GMP thread has scanned any paths in MOZ_GMP_PATH.
    nsCOMPtr<nsIThread> thread;
    nsresult rv = GetThread(getter_AddRefs(thread));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = NS_DispatchAndSpinEventLoopUntilComplete(
        "GeckoMediaPluginServiceParent::EnsurePluginsOnDiskScanned"_ns, thread,
        MakeAndAddRef<mozilla::Runnable>("GMPDummyRunnable"));
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(mScannedPluginOnDisk, "Should have scanned MOZ_GMP_PATH by now");
  }

  return NS_OK;
}

already_AddRefed<GMPParent> GeckoMediaPluginServiceParent::FindPluginForAPIFrom(
    size_t aSearchStartIndex, const nsACString& aAPI,
    const nsTArray<nsCString>& aTags, size_t* aOutPluginIndex) {
  mMutex.AssertCurrentThreadOwns();
  for (size_t i = aSearchStartIndex; i < mPlugins.Length(); i++) {
    RefPtr<GMPParent> gmp = mPlugins[i];
    if (!GMPCapability::Supports(gmp->GetCapabilities(), aAPI, aTags)) {
      continue;
    }
    if (aOutPluginIndex) {
      *aOutPluginIndex = i;
    }
    return gmp.forget();
  }
  return nullptr;
}

already_AddRefed<GMPParent> GeckoMediaPluginServiceParent::SelectPluginForAPI(
    const nsACString& aNodeId, const nsACString& aAPI,
    const nsTArray<nsCString>& aTags) {
  AssertOnGMPThread();

  GMPParent* gmpToClone = nullptr;
  {
    MutexAutoLock lock(mMutex);
    size_t index = 0;
    RefPtr<GMPParent> gmp;
    while ((gmp = FindPluginForAPIFrom(index, aAPI, aTags, &index))) {
      if (aNodeId.IsEmpty()) {
        if (gmp->CanBeSharedCrossNodeIds()) {
          return gmp.forget();
        }
      } else if (gmp->CanBeUsedFrom(aNodeId)) {
        return gmp.forget();
      }

      if (!gmpToClone ||
          (gmpToClone->IsMarkedForDeletion() && !gmp->IsMarkedForDeletion())) {
        // This GMP has the correct type but has the wrong nodeId; hold on to it
        // in case we need to clone it.
        // Prefer GMPs in-use for the case where an upgraded plugin version is
        // waiting for the old one to die. If the old plugin is in use, we
        // should continue using it so that any persistent state remains
        // consistent. Otherwise, just check that the plugin isn't scheduled
        // for deletion.
        gmpToClone = gmp;
      }
      // Loop around and try the next plugin; it may be usable from aNodeId.
      index++;
    }
  }

  // Plugin exists, but we can't use it due to cross-origin separation. Create a
  // new one.
  if (gmpToClone) {
    RefPtr<GMPParent> clone = ClonePlugin(gmpToClone);
    {
      MutexAutoLock lock(mMutex);
      mPlugins.AppendElement(clone);
    }
    if (!aNodeId.IsEmpty()) {
      clone->SetNodeId(aNodeId);
    }
    return clone.forget();
  }

  return nullptr;
}

static already_AddRefed<GMPParent> CreateGMPParent() {
  // Should run on the GMP thread.
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  if (!SandboxInfo::Get().CanSandboxMedia()) {
    if (!StaticPrefs::media_gmp_insecure_allow()) {
      NS_WARNING("Denying media plugin load due to lack of sandboxing.");
      return nullptr;
    }
    NS_WARNING("Loading media plugin despite lack of sandboxing.");
  }
#endif
  RefPtr<GMPParent> gmpParent = new GMPParent();
  return gmpParent.forget();
}

already_AddRefed<GMPParent> GeckoMediaPluginServiceParent::ClonePlugin(
    const GMPParent* aOriginal) {
  AssertOnGMPThread();
  MOZ_ASSERT(aOriginal);

  RefPtr<GMPParent> gmp = CreateGMPParent();
  if (!gmp) {
    NS_WARNING("Can't Create GMPParent");
    return nullptr;
  }

  gmp->CloneFrom(aOriginal);
  return gmp.forget();
}

RefPtr<GenericPromise> GeckoMediaPluginServiceParent::AddOnGMPThread(
    nsString aDirectory) {
#ifdef XP_WIN
  // On Windows our various test harnesses often pass paths with UNIX dir
  // separators, or a mix of dir separators. NS_NewLocalFile() can't handle
  // that, so fixup to match the platform's expected format. This makes us
  // more robust in the face of bad input and test harnesses changing...
  std::replace(aDirectory.BeginWriting(), aDirectory.EndWriting(), '/', '\\');
#endif

  AssertOnGMPThread();
  nsCString dir = NS_ConvertUTF16toUTF8(aDirectory);
  nsCOMPtr<nsISerialEventTarget> thread(GetGMPThread());
  if (!thread) {
    GMP_LOG_DEBUG("%s::%s: %s No GMP Thread", __CLASS__, __FUNCTION__,
                  dir.get());
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  GMP_LOG_DEBUG("%s::%s: %s", __CLASS__, __FUNCTION__, dir.get());

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG("%s::%s: failed to create nsIFile for dir=%s rv=%" PRIx32,
                  __CLASS__, __FUNCTION__, dir.get(),
                  static_cast<uint32_t>(rv));
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GMPParent> gmp = CreateGMPParent();
  if (!gmp) {
    NS_WARNING("Can't Create GMPParent");
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  return gmp->Init(this, directory)
      ->Then(
          thread, __func__,
          [gmp, self, dir](bool aVal) {
            GMP_LOG_DEBUG("%s::%s: %s Succeeded", __CLASS__, __FUNCTION__,
                          dir.get());
            {
              MutexAutoLock lock(self->mMutex);
              self->mPlugins.AppendElement(gmp);
            }
            return GenericPromise::CreateAndResolve(aVal, __func__);
          },
          [dir](nsresult aResult) {
            GMP_LOG_DEBUG("%s::%s: %s Failed", __CLASS__, __FUNCTION__,
                          dir.get());
            return GenericPromise::CreateAndReject(aResult, __func__);
          });
}

void GeckoMediaPluginServiceParent::RemoveOnGMPThread(
    const nsAString& aDirectory, const bool aDeleteFromDisk,
    const bool aCanDefer) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: %s", __CLASS__, __FUNCTION__,
                NS_LossyConvertUTF16toASCII(aDirectory).get());

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Plugin destruction can modify |mPlugins|. Put them aside for now and
  // destroy them once we're done with |mPlugins|.
  nsTArray<RefPtr<GMPParent>> deadPlugins;

  bool inUse = false;
  MutexAutoLock lock(mMutex);
  for (size_t i = mPlugins.Length(); i-- > 0;) {
    nsCOMPtr<nsIFile> pluginpath = mPlugins[i]->GetDirectory();
    bool equals;
    if (NS_FAILED(directory->Equals(pluginpath, &equals)) || !equals) {
      continue;
    }

    RefPtr<GMPParent> gmp = mPlugins[i];
    if (aDeleteFromDisk && gmp->State() != GMPState::NotLoaded) {
      // We have to wait for the child process to release its lib handle
      // before we can delete the GMP.
      inUse = true;
      gmp->MarkForDeletion();

      if (!mPluginsWaitingForDeletion.Contains(aDirectory)) {
        mPluginsWaitingForDeletion.AppendElement(aDirectory);
      }
    }

    if (gmp->State() == GMPState::NotLoaded || !aCanDefer) {
      // GMP not in use or shutdown is being forced; can shut it down now.
      deadPlugins.AppendElement(gmp);
      mPlugins.RemoveElementAt(i);
    }
  }

  {
    MutexAutoUnlock unlock(mMutex);
    for (auto& gmp : deadPlugins) {
      gmp->CloseActive(true);
    }
  }

  if (aDeleteFromDisk && !inUse) {
    // Ensure the GMP dir and all files in it are writable, so we have
    // permission to delete them.
    directory->SetPermissions(0700);
    DirectoryEnumerator iter(directory, DirectoryEnumerator::FilesAndDirs);
    for (nsCOMPtr<nsIFile> dirEntry; (dirEntry = iter.Next()) != nullptr;) {
      dirEntry->SetPermissions(0700);
    }
    if (NS_SUCCEEDED(directory->Remove(true))) {
      mPluginsWaitingForDeletion.RemoveElement(aDirectory);
      nsCOMPtr<nsIRunnable> task = new NotifyObserversTask(
          "gmp-directory-deleted", nsString(aDirectory));
      mMainThread->Dispatch(task.forget());
    }
  }
}  // Ignore mutex not held; MutexAutoUnlock is used above

// May remove when Bug 1043671 is fixed
static void Dummy(RefPtr<GMPParent> aOnDeathsDoor) {
  // exists solely to do nothing and let the Runnable kill the GMPParent
  // when done.
}

void GeckoMediaPluginServiceParent::PluginTerminated(
    const RefPtr<GMPParent>& aPlugin) {
  AssertOnGMPThread();

  if (aPlugin->IsMarkedForDeletion()) {
    nsString path;
    RefPtr<nsIFile> dir = aPlugin->GetDirectory();
    nsresult rv = dir->GetPath(path);
    NS_ENSURE_SUCCESS_VOID(rv);
    if (mPluginsWaitingForDeletion.Contains(path)) {
      RemoveOnGMPThread(path, true /* delete */, true /* can defer */);
    }
  }
}

void GeckoMediaPluginServiceParent::ReAddOnGMPThread(
    const RefPtr<GMPParent>& aOld) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, (void*)aOld);

  RefPtr<GMPParent> gmp;
  if (!mShuttingDownOnGMPThread) {
    // We're not shutting down, so replace the old plugin in the list with a
    // clone which is in a pristine state. Note: We place the plugin in
    // the same slot in the array as a hack to ensure if we re-request with
    // the same capabilities we get an instance of the same plugin.
    gmp = ClonePlugin(aOld);
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mPlugins.Contains(aOld));
    if (mPlugins.Contains(aOld)) {
      mPlugins[mPlugins.IndexOf(aOld)] = gmp;
    }
  } else {
    // We're shutting down; don't re-add plugin, let the old plugin die.
    MutexAutoLock lock(mMutex);
    mPlugins.RemoveElement(aOld);
  }
  // Schedule aOld to be destroyed.  We can't destroy it from here since we
  // may be inside ActorDestroyed() for it.
  NS_DispatchToCurrentThread(WrapRunnableNM(&Dummy, aOld));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetStorageDir(nsIFile** aOutFile) {
  if (NS_WARN_IF(!mStorageBaseDir)) {
    return NS_ERROR_FAILURE;
  }
  return mStorageBaseDir->Clone(aOutFile);
}

nsresult WriteToFile(nsIFile* aPath, const nsACString& aFileName,
                     const nsACString& aData) {
  nsCOMPtr<nsIFile> path;
  nsresult rv = aPath->Clone(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->AppendNative(aFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PRFileDesc* f = nullptr;
  rv = path->OpenNSPRFileDesc(PR_WRONLY | PR_CREATE_FILE, PR_IRWXU, &f);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t len = PR_Write(f, aData.BeginReading(), aData.Length());
  PR_Close(f);
  if (NS_WARN_IF(len < 0 || (size_t)len != aData.Length())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult ReadFromFile(nsIFile* aPath, const nsACString& aFileName,
                             nsACString& aOutData, int32_t aMaxLength) {
  nsCOMPtr<nsIFile> path;
  nsresult rv = aPath->Clone(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->AppendNative(aFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PRFileDesc* f = nullptr;
  rv = path->OpenNSPRFileDesc(PR_RDONLY | PR_CREATE_FILE, PR_IRWXU, &f);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  auto size = PR_Seek(f, 0, PR_SEEK_END);
  PR_Seek(f, 0, PR_SEEK_SET);

  if (size > aMaxLength) {
    return NS_ERROR_FAILURE;
  }
  aOutData.SetLength(size);

  auto len = PR_Read(f, aOutData.BeginWriting(), size);
  PR_Close(f);
  if (NS_WARN_IF(len != size)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult ReadSalt(nsIFile* aPath, nsACString& aOutData) {
  return ReadFromFile(aPath, "salt"_ns, aOutData, NodeIdSaltLength);
}

already_AddRefed<GMPStorage> GeckoMediaPluginServiceParent::GetMemoryStorageFor(
    const nsACString& aNodeId) {
  return do_AddRef(mTempGMPStorage.LookupOrInsertWith(
      aNodeId, [] { return CreateGMPMemoryStorage(); }));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::IsPersistentStorageAllowed(
    const nsACString& aNodeId, bool* aOutAllowed) {
  AssertOnGMPThread();
  NS_ENSURE_ARG(aOutAllowed);
  // We disallow persistent storage for the NodeId used for shared GMP
  // decoding, to prevent GMP decoding being used to track what a user
  // watches somehow.
  *aOutAllowed = !aNodeId.Equals(SHARED_GMP_DECODING_NODE_ID) &&
                 mPersistentStorageAllowed.Get(aNodeId);
  return NS_OK;
}

nsresult GeckoMediaPluginServiceParent::GetNodeId(
    const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
    const nsAString& aGMPName, nsACString& aOutId) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: (%s, %s)", __CLASS__, __FUNCTION__,
                NS_ConvertUTF16toUTF8(aOrigin).get(),
                NS_ConvertUTF16toUTF8(aTopLevelOrigin).get());

  nsresult rv;

  if (aOrigin.EqualsLiteral("null") || aOrigin.IsEmpty() ||
      aTopLevelOrigin.EqualsLiteral("null") || aTopLevelOrigin.IsEmpty()) {
    // (origin, topLevelOrigin) is null or empty; this is for an anonymous
    // origin, probably a local file, for which we don't provide persistent
    // storage. Generate a random node id, and don't store it so that the GMP's
    // storage is temporary and the process for this GMP is not shared with GMP
    // instances that have the same nodeId.
    nsAutoCString salt;
    rv = GenerateRandomPathName(salt, NodeIdSaltLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aOutId = salt;
    mPersistentStorageAllowed.InsertOrUpdate(salt, false);
    return NS_OK;
  }

  const uint32_t hash =
      AddToHash(HashString(aOrigin), HashString(aTopLevelOrigin));

  if (OriginAttributes::IsPrivateBrowsing(NS_ConvertUTF16toUTF8(aOrigin))) {
    // For PB mode, we store the node id, indexed by the origin pair and GMP
    // name, so that if the same origin pair is opened for the same GMP in this
    // session, it gets the same node id.
    const uint32_t pbHash = AddToHash(HashString(aGMPName), hash);
    return mTempNodeIds.WithEntryHandle(pbHash, [&](auto&& entry) {
      if (!entry) {
        // No salt stored, generate and temporarily store some for this id.
        nsAutoCString newSalt;
        rv = GenerateRandomPathName(newSalt, NodeIdSaltLength);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        auto salt = MakeUnique<nsCString>(newSalt);

        mPersistentStorageAllowed.InsertOrUpdate(*salt, false);

        entry.Insert(std::move(salt));
      }

      aOutId = *entry.Data();
      return NS_OK;
    });
  }

  // Otherwise, try to see if we've previously generated and stored salt
  // for this origin pair.
  nsCOMPtr<nsIFile> path;  // $profileDir/gmp/$platform/
  rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // $profileDir/gmp/$platform/$gmpName/
  rv = path->Append(aGMPName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // $profileDir/gmp/$platform/$gmpName/id/
  rv = path->AppendNative("id"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString hashStr;
  hashStr.AppendInt((int64_t)hash);

  // $profileDir/gmp/$platform/$gmpName/id/$hash
  rv = path->AppendNative(hashStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> saltFile;
  rv = path->Clone(getter_AddRefs(saltFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = saltFile->AppendNative("salt"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString salt;
  bool exists = false;
  rv = saltFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!exists) {
    // No stored salt for this origin. Generate salt, and store it and
    // the origin on disk.
    nsresult rv = GenerateRandomPathName(salt, NodeIdSaltLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(salt.Length() == NodeIdSaltLength);

    // $profileDir/gmp/$platform/$gmpName/id/$hash/salt
    rv = WriteToFile(path, "salt"_ns, salt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/$platform/$gmpName/id/$hash/origin
    rv = WriteToFile(path, "origin"_ns, NS_ConvertUTF16toUTF8(aOrigin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/$platform/$gmpName/id/$hash/topLevelOrigin
    rv = WriteToFile(path, "topLevelOrigin"_ns,
                     NS_ConvertUTF16toUTF8(aTopLevelOrigin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

  } else {
    rv = ReadSalt(path, salt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  aOutId = salt;
  mPersistentStorageAllowed.InsertOrUpdate(salt, true);

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetNodeId(
    const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
    const nsAString& aGMPName, UniquePtr<GetNodeIdCallback>&& aCallback) {
  nsCString nodeId;
  nsresult rv = GetNodeId(aOrigin, aTopLevelOrigin, aGMPName, nodeId);
  aCallback->Done(rv, nodeId);
  return rv;
}

nsresult GeckoMediaPluginServiceParent::GetNodeId(
    const NodeIdVariant& aNodeIdVariant, nsACString& aOutId) {
  if (aNodeIdVariant.type() == NodeIdVariant::TnsCString) {
    // The union already contains a node ID as a string, use that.
    aOutId = aNodeIdVariant.get_nsCString();
    return NS_OK;
  }
  // The union contains a NodeIdParts, convert it to a node ID string.
  NodeIdParts nodeIdParts{aNodeIdVariant.get_NodeIdParts()};
  return GetNodeId(nodeIdParts.mOrigin(), nodeIdParts.mTopLevelOrigin(),
                   nodeIdParts.mGMPName(), aOutId);
}

static bool ExtractHostName(const nsACString& aOrigin, nsACString& aOutData) {
  nsCString str;
  str.Assign(aOrigin);
  int begin = str.Find("://");
  // The scheme is missing!
  if (begin == -1) {
    return false;
  }

  int end = str.RFind(":");
  // Remove the port number
  if (end != begin) {
    str.SetLength(end);
  }

  nsDependentCSubstring host(str, begin + 3);
  aOutData.Assign(host);
  return true;
}

constexpr uint32_t kMaxDomainLength = 253;
// http://en.wikipedia.org/wiki/Domain_Name_System#Domain_name_syntax

constexpr std::array<nsLiteralCString, 2> kFileNames = {"origin"_ns,
                                                        "topLevelOrigin"_ns};

bool MatchOrigin(nsIFile* aPath, const nsACString& aSite,
                 const mozilla::OriginAttributesPattern& aPattern) {
  nsresult rv;
  nsCString str;
  nsCString originNoSuffix;
  for (const auto& fileName : kFileNames) {
    rv = ReadFromFile(aPath, fileName, str, kMaxDomainLength);
    mozilla::OriginAttributes originAttributes;
    if (NS_FAILED(rv) ||
        !originAttributes.PopulateFromOrigin(str, originNoSuffix)) {
      // Fails on parsing the originAttributes, treat this as a non-match.
      return false;
    }

    if (ExtractHostName(originNoSuffix, str) && str.Equals(aSite) &&
        aPattern.Matches(originAttributes)) {
      return true;
    }
  }
  return false;
}

bool MatchBaseDomain(nsIFile* aPath, const nsACString& aBaseDomain) {
  nsresult rv;
  nsCString fileContent;
  nsCString originNoSuffix;
  for (const auto& fileName : kFileNames) {
    rv = ReadFromFile(aPath, fileName, fileContent, kMaxDomainLength);
    mozilla::OriginAttributes originAttributes;
    if (NS_FAILED(rv) ||
        !originAttributes.PopulateFromOrigin(fileContent, originNoSuffix)) {
      // Fails on parsing the originAttributes, treat this as a non-match.
      return false;
    }
    nsCString originHostname;
    if (!ExtractHostName(originNoSuffix, originHostname)) {
      return false;
    }
    bool success;
    rv = net::HasRootDomain(originHostname, aBaseDomain, &success);
    if (NS_SUCCEEDED(rv) && success) {
      return true;
    }
  }
  return false;
}

template <typename T>
static void KillPlugins(const nsTArray<RefPtr<GMPParent>>& aPlugins,
                        Mutex& aMutex, T&& aFilter) {
  // Shutdown the plugins when |aFilter| evaluates to true.
  // After we clear storage data, node IDs will become invalid and shouldn't be
  // used anymore. We need to kill plugins with such nodeIDs.
  // Note: we can't shut them down while holding the lock,
  // as the lock is not re-entrant and shutdown requires taking the lock.
  // The plugin list is only edited on the GMP thread, so this should be OK.
  nsTArray<RefPtr<GMPParent>> pluginsToKill;
  {
    MutexAutoLock lock(aMutex);
    for (size_t i = 0; i < aPlugins.Length(); i++) {
      RefPtr<GMPParent> parent(aPlugins[i]);
      if (aFilter(parent)) {
        pluginsToKill.AppendElement(parent);
      }
    }
  }

  for (size_t i = 0; i < pluginsToKill.Length(); i++) {
    pluginsToKill[i]->CloseActive(false);
  }
}

struct NodeFilter {
  explicit NodeFilter(const nsTArray<nsCString>& nodeIDs) : mNodeIDs(nodeIDs) {}
  bool operator()(GMPParent* aParent) {
    return mNodeIDs.Contains(aParent->GetNodeId());
  }

 private:
  const nsTArray<nsCString>& mNodeIDs;
};

void GeckoMediaPluginServiceParent::ClearNodeIdAndPlugin(
    DirectoryFilter& aFilter) {
  // $profileDir/gmp/$platform/
  nsCOMPtr<nsIFile> path;
  nsresult rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Iterate all sub-folders of $profileDir/gmp/$platform/, i.e. the dirs in
  // which specific GMPs store their data.
  DirectoryEnumerator iter(path, DirectoryEnumerator::DirsOnly);
  for (nsCOMPtr<nsIFile> pluginDir; (pluginDir = iter.Next()) != nullptr;) {
    ClearNodeIdAndPlugin(pluginDir, aFilter);
  }
}

void GeckoMediaPluginServiceParent::ClearNodeIdAndPlugin(
    nsIFile* aPluginStorageDir, DirectoryFilter& aFilter) {
  // $profileDir/gmp/$platform/$gmpName/id/
  nsCOMPtr<nsIFile> path = CloneAndAppend(aPluginStorageDir, u"id"_ns);
  if (!path) {
    return;
  }

  // Iterate all sub-folders of $profileDir/gmp/$platform/$gmpName/id/
  nsTArray<nsCString> nodeIDsToClear;
  DirectoryEnumerator iter(path, DirectoryEnumerator::DirsOnly);
  for (nsCOMPtr<nsIFile> dirEntry; (dirEntry = iter.Next()) != nullptr;) {
    // dirEntry is the hash of origins, i.e.:
    // $profileDir/gmp/$platform/$gmpName/id/$originHash/
    if (!aFilter(dirEntry)) {
      continue;
    }
    nsAutoCString salt;
    if (NS_SUCCEEDED(ReadSalt(dirEntry, salt))) {
      // Keep node IDs to clear data/plugins associated with them later.
      nodeIDsToClear.AppendElement(salt);
      // Also remove node IDs from the table.
      mPersistentStorageAllowed.Remove(salt);
    }
    // Now we can remove the directory for the origin pair.
    if (NS_FAILED(dirEntry->Remove(true))) {
      NS_WARNING("Failed to delete the directory for the origin pair");
    }
  }

  // Kill plugin instances that have node IDs being cleared.
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  KillPlugins(mPlugins, mMutex, NodeFilter(nodeIDsToClear));
  MOZ_POP_THREAD_SAFETY

  // Clear all storage in $profileDir/gmp/$platform/$gmpName/storage/$nodeId/
  path = CloneAndAppend(aPluginStorageDir, u"storage"_ns);
  if (!path) {
    return;
  }

  for (const nsACString& nodeId : nodeIDsToClear) {
    nsCOMPtr<nsIFile> dirEntry;
    nsresult rv = path->Clone(getter_AddRefs(dirEntry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    rv = dirEntry->AppendNative(nodeId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    if (NS_FAILED(dirEntry->Remove(true))) {
      NS_WARNING("Failed to delete GMP storage directory for the node");
    }
  }
}

void GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread(
    const nsACString& aSite, const mozilla::OriginAttributesPattern& aPattern) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: origin=%s", __CLASS__, __FUNCTION__, aSite.Data());

  struct OriginFilter : public DirectoryFilter {
    explicit OriginFilter(const nsACString& aSite,
                          const mozilla::OriginAttributesPattern& aPattern)
        : mSite(aSite), mPattern(aPattern) {}
    bool operator()(nsIFile* aPath) override {
      return MatchOrigin(aPath, mSite, mPattern);
    }

   private:
    const nsACString& mSite;
    const mozilla::OriginAttributesPattern& mPattern;
  } filter(aSite, aPattern);

  ClearNodeIdAndPlugin(filter);
}

void GeckoMediaPluginServiceParent::ForgetThisBaseDomainOnGMPThread(
    const nsACString& aBaseDomain) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: baseDomain=%s", __CLASS__, __FUNCTION__,
                aBaseDomain.Data());

  struct BaseDomainFilter : public DirectoryFilter {
    explicit BaseDomainFilter(const nsACString& aBaseDomain)
        : mBaseDomain(aBaseDomain) {}
    bool operator()(nsIFile* aPath) override {
      return MatchBaseDomain(aPath, mBaseDomain);
    }

   private:
    const nsACString& mBaseDomain;
  } filter(aBaseDomain);

  ClearNodeIdAndPlugin(filter);
}

void GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread(
    PRTime aSince) {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s: since=%" PRId64, __CLASS__, __FUNCTION__,
                (int64_t)aSince);

  struct MTimeFilter : public DirectoryFilter {
    explicit MTimeFilter(PRTime aSince) : mSince(aSince) {}

    // Return true if any files under aPath is modified after |mSince|.
    bool IsModifiedAfter(nsIFile* aPath) {
      PRTime lastModified;
      nsresult rv = aPath->GetLastModifiedTime(&lastModified);
      if (NS_SUCCEEDED(rv) && lastModified >= mSince) {
        return true;
      }
      DirectoryEnumerator iter(aPath, DirectoryEnumerator::FilesAndDirs);
      for (nsCOMPtr<nsIFile> dirEntry; (dirEntry = iter.Next()) != nullptr;) {
        if (IsModifiedAfter(dirEntry)) {
          return true;
        }
      }
      return false;
    }

    // |aPath| is $profileDir/gmp/$platform/$gmpName/id/$originHash/
    bool operator()(nsIFile* aPath) override {
      if (IsModifiedAfter(aPath)) {
        return true;
      }

      nsAutoCString salt;
      if (NS_WARN_IF(NS_FAILED(ReadSalt(aPath, salt)))) {
        return false;
      }

      // $profileDir/gmp/$platform/$gmpName/id/
      nsCOMPtr<nsIFile> idDir;
      if (NS_WARN_IF(NS_FAILED(aPath->GetParent(getter_AddRefs(idDir))))) {
        return false;
      }
      // $profileDir/gmp/$platform/$gmpName/
      nsCOMPtr<nsIFile> temp;
      if (NS_WARN_IF(NS_FAILED(idDir->GetParent(getter_AddRefs(temp))))) {
        return false;
      }

      // $profileDir/gmp/$platform/$gmpName/storage/
      if (NS_WARN_IF(NS_FAILED(temp->Append(u"storage"_ns)))) {
        return false;
      }
      // $profileDir/gmp/$platform/$gmpName/storage/$originSalt
      return NS_SUCCEEDED(temp->AppendNative(salt)) && IsModifiedAfter(temp);
    }

   private:
    const PRTime mSince;
  } filter(aSince);

  ClearNodeIdAndPlugin(filter);

  nsCOMPtr<nsIRunnable> task =
      new NotifyObserversTask("gmp-clear-storage-complete");
  mMainThread->Dispatch(task.forget());
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::ForgetThisSite(const nsAString& aSite,
                                              const nsAString& aPattern) {
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::OriginAttributesPattern pattern;

  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  return ForgetThisSiteNative(aSite, pattern);
}

nsresult GeckoMediaPluginServiceParent::ForgetThisSiteNative(
    const nsAString& aSite, const mozilla::OriginAttributesPattern& aPattern) {
  MOZ_ASSERT(NS_IsMainThread());

  return GMPDispatch(
      NewRunnableMethod<nsCString, mozilla::OriginAttributesPattern>(
          "gmp::GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread", this,
          &GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread,
          NS_ConvertUTF16toUTF8(aSite), aPattern));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::ForgetThisBaseDomain(
    const nsAString& aBaseDomain) {
  MOZ_ASSERT(NS_IsMainThread());

  return ForgetThisBaseDomainNative(aBaseDomain);
}

nsresult GeckoMediaPluginServiceParent::ForgetThisBaseDomainNative(
    const nsAString& aBaseDomain) {
  MOZ_ASSERT(NS_IsMainThread());

  return GMPDispatch(NewRunnableMethod<nsCString>(
      "gmp::GeckoMediaPluginServiceParent::ForgetThisBaseDomainOnGMPThread",
      this, &GeckoMediaPluginServiceParent::ForgetThisBaseDomainOnGMPThread,
      NS_ConvertUTF16toUTF8(aBaseDomain)));
}

static bool IsNodeIdValid(GMPParent* aParent) {
  return !aParent->GetNodeId().IsEmpty();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetName(nsAString& aName) {
  aName = u"GeckoMediaPluginServiceParent: shutdown"_ns;
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetState(nsIPropertyBag**) { return NS_OK; }

NS_IMETHODIMP
GeckoMediaPluginServiceParent::BlockShutdown(nsIAsyncShutdownClient*) {
  return NS_OK;
}

// Called from GMPServiceParent::Create() which holds the lock
void GeckoMediaPluginServiceParent::ServiceUserCreated(
    GMPServiceParent* aServiceParent) {
  MOZ_ASSERT(NS_IsMainThread());
  mMutex.AssertCurrentThreadOwns();

  MOZ_ASSERT(!mServiceParents.Contains(aServiceParent));
  mServiceParents.AppendElement(aServiceParent);
  if (mServiceParents.Length() == 1) {
    nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
    MOZ_RELEASE_ASSERT(barrier);

    nsresult rv = barrier->AddBlocker(
        this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
        u"GeckoMediaPluginServiceParent shutdown"_ns);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
}

void GeckoMediaPluginServiceParent::ServiceUserDestroyed(
    GMPServiceParent* aServiceParent) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mServiceParents.Length() > 0);
  MOZ_ASSERT(mServiceParents.Contains(aServiceParent));
  mServiceParents.RemoveElement(aServiceParent);
  if (mServiceParents.IsEmpty()) {
    nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
    MOZ_RELEASE_ASSERT(barrier);
    nsresult rv = barrier->RemoveBlocker(this);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
}

void GeckoMediaPluginServiceParent::ClearStorage() {
  AssertOnGMPThread();
  GMP_LOG_DEBUG("%s::%s", __CLASS__, __FUNCTION__);

  // Kill plugins with valid nodeIDs.
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  KillPlugins(mPlugins, mMutex, &IsNodeIdValid);
  MOZ_POP_THREAD_SAFETY

  nsCOMPtr<nsIFile> path;  // $profileDir/gmp/$platform/
  nsresult rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_FAILED(path->Remove(true))) {
    NS_WARNING("Failed to delete GMP storage directory");
  }

  // Clear private-browsing storage.
  mTempGMPStorage.Clear();

  nsCOMPtr<nsIRunnable> task =
      new NotifyObserversTask("gmp-clear-storage-complete");
  mMainThread->Dispatch(task.forget());
}

already_AddRefed<GMPParent> GeckoMediaPluginServiceParent::GetById(
    uint32_t aPluginId) {
  MutexAutoLock lock(mMutex);
  for (const RefPtr<GMPParent>& gmp : mPlugins) {
    if (gmp->GetPluginId() == aPluginId) {
      return do_AddRef(gmp);
    }
  }
  return nullptr;
}

GMPServiceParent::GMPServiceParent(GeckoMediaPluginServiceParent* aService)
    : mService(aService) {
  MOZ_ASSERT(NS_IsMainThread(), "Should be constructed on the main thread");
  MOZ_ASSERT(mService);
  mService->ServiceUserCreated(this);
}

GMPServiceParent::~GMPServiceParent() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be destroyted on the main thread");
  MOZ_ASSERT(mService);
  mService->ServiceUserDestroyed(this);
}

mozilla::ipc::IPCResult GMPServiceParent::RecvLaunchGMP(
    const NodeIdVariant& aNodeIdVariant, const nsACString& aAPI,
    nsTArray<nsCString>&& aTags, nsTArray<ProcessId>&& aAlreadyBridgedTo,
    LaunchGMPResolver&& aResolve) {
  GMPLaunchResult result;

  if (mService->IsShuttingDown()) {
    result.pluginId() = 0;
    result.pluginType() = GMPPluginType::Unknown;
    result.pid() = base::kInvalidProcessId;
    result.result() = NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    result.errorDescription() = "Service is shutting down."_ns;
    aResolve(std::move(result));
    return IPC_OK();
  }

  nsCString nodeIdString;
  nsresult rv = mService->GetNodeId(aNodeIdVariant, nodeIdString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    result.pluginId() = 0;
    result.pluginType() = GMPPluginType::Unknown;
    result.pid() = base::kInvalidProcessId;
    result.result() = rv;
    result.errorDescription() = "GetNodeId failed."_ns;
    aResolve(std::move(result));
    return IPC_OK();
  }

  RefPtr<GMPParent> gmp =
      mService->SelectPluginForAPI(nodeIdString, aAPI, aTags);
  if (gmp) {
    result.pluginId() = gmp->GetPluginId();
    result.pluginType() = gmp->GetPluginType();
  } else {
    result.pluginId() = 0;
    result.pluginType() = GMPPluginType::Unknown;
    result.pid() = base::kInvalidProcessId;
    result.result() = NS_ERROR_FAILURE;
    result.errorDescription() = "SelectPluginForAPI returns nullptr."_ns;
    aResolve(std::move(result));
    return IPC_OK();
  }

  if (!gmp->EnsureProcessLoaded(&result.pid())) {
    result.pid() = base::kInvalidProcessId;
    result.result() = NS_ERROR_FAILURE;
    result.errorDescription() = "Process has not loaded."_ns;
    aResolve(std::move(result));
    return IPC_OK();
  }

  MOZ_ASSERT(result.pid() != base::kInvalidProcessId);

  result.displayName() = gmp->GetDisplayName();

  if (aAlreadyBridgedTo.Contains(result.pid())) {
    result.result() = NS_OK;
    aResolve(std::move(result));
    return IPC_OK();
  }

  Endpoint<PGMPContentParent> parent;
  Endpoint<PGMPContentChild> child;
  rv = PGMPContent::CreateEndpoints(OtherPid(), result.pid(), &parent, &child);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    result.result() = rv;
    result.errorDescription() = "PGMPContent::CreateEndpoints failed."_ns;
    aResolve(std::move(result));
    return IPC_OK();
  }

  if (!gmp->SendInitGMPContentChild(std::move(child))) {
    result.result() = NS_ERROR_FAILURE;
    result.errorDescription() = "SendInitGMPContentChild failed."_ns;
    return IPC_OK();
  }

  gmp->IncrementGMPContentChildCount();

  result.result() = NS_OK;
  result.endpoint() = std::move(parent);
  aResolve(std::move(result));
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPServiceParent::RecvGetGMPNodeId(
    const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
    const nsAString& aGMPName, GetGMPNodeIdResolver&& aResolve) {
  nsCString id;
  nsresult rv = mService->GetNodeId(aOrigin, aTopLevelOrigin, aGMPName, id);
  aResolve(id);
  if (!NS_SUCCEEDED(rv)) {
    return IPC_FAIL(
        this,
        "GMPServiceParent::RecvGetGMPNodeId: mService->GetNodeId failed.");
  }
  return IPC_OK();
}

class OpenPGMPServiceParent : public mozilla::Runnable {
 public:
  OpenPGMPServiceParent(RefPtr<GMPServiceParent>&& aGMPServiceParent,
                        ipc::Endpoint<PGMPServiceParent>&& aEndpoint,
                        bool* aResult)
      : Runnable("gmp::OpenPGMPServiceParent"),
        mGMPServiceParent(aGMPServiceParent),
        mEndpoint(std::move(aEndpoint)),
        mResult(aResult) {}

  NS_IMETHOD Run() override {
    *mResult = mEndpoint.Bind(mGMPServiceParent);
    return NS_OK;
  }

 private:
  RefPtr<GMPServiceParent> mGMPServiceParent;
  ipc::Endpoint<PGMPServiceParent> mEndpoint;
  bool* mResult;
};

/* static */
bool GMPServiceParent::Create(Endpoint<PGMPServiceParent>&& aGMPService) {
  RefPtr<GeckoMediaPluginServiceParent> gmp =
      GeckoMediaPluginServiceParent::GetSingleton();

  if (!gmp || gmp->mShuttingDown) {
    // Shutdown is initiated. There is no point creating a new actor.
    return false;
  }

  nsCOMPtr<nsIThread> gmpThread;
  RefPtr<GMPServiceParent> serviceParent;
  {
    MutexAutoLock lock(gmp->mMutex);
    nsresult rv = gmp->GetThreadLocked(getter_AddRefs(gmpThread));
    NS_ENSURE_SUCCESS(rv, false);
    serviceParent = new GMPServiceParent(gmp);
  }
  bool ok;
  nsresult rv = NS_DispatchAndSpinEventLoopUntilComplete(
      "GMPServiceParent::Create"_ns, gmpThread,
      do_AddRef(new OpenPGMPServiceParent(std::move(serviceParent),
                                          std::move(aGMPService), &ok)));

  if (NS_WARN_IF(NS_FAILED(rv) || !ok)) {
    return false;
  }

  // Now that the service parent is set up, it will be released by IPC
  // refcounting, so we don't need to hold any more references here.

  return true;
}

}  // namespace mozilla::gmp

#undef __CLASS__

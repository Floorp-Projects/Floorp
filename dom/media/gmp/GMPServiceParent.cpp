/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceParent.h"
#include "GMPService.h"
#include "prio.h"
#include "base/task.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/ContentParent.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "nsAutoPtr.h"
#include "nsIObserverService.h"
#include "GeckoChildProcessHost.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/SyncRunnable.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/Services.h"
#include "nsNativeCharsetUtils.h"
#include "nsIConsoleService.h"
#include "mozilla/Unused.h"
#include "GMPDecryptorParent.h"
#include "nsComponentManagerUtils.h"
#include "runnable_utils.h"
#include "VideoUtils.h"
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/SandboxInfo.h"
#endif
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#endif
#include "nsIXULRuntime.h"
#include "GMPDecoderModule.h"
#include <limits>
#include "MediaPrefs.h"

using mozilla::ipc::Transport;

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPService"

namespace gmp {

static const uint32_t NodeIdSaltLength = 32;

already_AddRefed<GeckoMediaPluginServiceParent>
GeckoMediaPluginServiceParent::GetSingleton()
{
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
  : mShuttingDown(false)
  , mScannedPluginOnDisk(false)
  , mWaitingForPluginsSyncShutdown(false)
  , mInitPromiseMonitor("GeckoMediaPluginServiceParent::mInitPromiseMonitor")
  , mLoadPluginsFromDiskComplete(false)
  , mMainThread(SystemGroup::AbstractMainThreadFor(TaskCategory::Other))
{
  MOZ_ASSERT(NS_IsMainThread());
  mInitPromise.SetMonitor(&mInitPromiseMonitor);
}

GeckoMediaPluginServiceParent::~GeckoMediaPluginServiceParent()
{
  MOZ_ASSERT(mPlugins.IsEmpty());
}

nsresult
GeckoMediaPluginServiceParent::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, "profile-change-teardown", false));
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, "last-pb-context-exited", false));
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, "browser:purge-session-history", false));

#ifdef DEBUG
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, "mediakeys-request", false));
#endif

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->AddObserver("media.gmp.plugin.crash", this, false);
  }

  nsresult rv = InitStorage();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Kick off scanning for plugins
  nsCOMPtr<nsIThread> thread;
  rv = GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Detect if GMP storage has an incompatible version, and if so nuke it.
  int32_t version = Preferences::GetInt("media.gmp.storage.version.observed", 0);
  int32_t expected = Preferences::GetInt("media.gmp.storage.version.expected", 0);
  if (version != expected) {
    Preferences::SetInt("media.gmp.storage.version.observed", expected);
    return GMPDispatch(
      NewRunnableMethod("gmp::GeckoMediaPluginServiceParent::ClearStorage",
                        this,
                        &GeckoMediaPluginServiceParent::ClearStorage));
  }
  return NS_OK;
}

already_AddRefed<nsIFile>
CloneAndAppend(nsIFile* aFile, const nsAString& aDir)
{
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

static nsresult
GMPPlatformString(nsAString& aOutPlatform)
{
  // Append the OS and arch so that we don't reuse the storage if the profile is
  // copied or used under a different bit-ness, or copied to another platform.
  nsCOMPtr<nsIXULRuntime> runtime = do_GetService("@mozilla.org/xre/runtime;1");
  if (!runtime) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString OS;
  nsresult rv = runtime->GetOS(OS);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString arch;
  rv = runtime->GetXPCOMABI(arch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString platform;
  platform.Append(OS);
  platform.AppendLiteral("_");
  platform.Append(arch);

  aOutPlatform = NS_ConvertUTF8toUTF16(platform);

  return NS_OK;
}

nsresult
GeckoMediaPluginServiceParent::InitStorage()
{
  MOZ_ASSERT(NS_IsMainThread());

  // GMP storage should be used in the chrome process only.
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  // Directory service is main thread only, so cache the profile dir here
  // so that we can use it off main thread.
#ifdef MOZ_WIDGET_GONK
  nsresult rv = NS_NewLocalFile(NS_LITERAL_STRING("/data/b2g/mozilla"), false, getter_AddRefs(mStorageBaseDir));
#else
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mStorageBaseDir));
#endif

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mStorageBaseDir->AppendNative(NS_LITERAL_CSTRING("gmp"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mStorageBaseDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)) {
    return rv;
  }

  nsCOMPtr<nsIFile> gmpDirWithoutPlatform;
  rv = mStorageBaseDir->Clone(getter_AddRefs(gmpDirWithoutPlatform));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString platform;
  rv = GMPPlatformString(platform);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = mStorageBaseDir->Append(platform);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mStorageBaseDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)) {
    return rv;
  }

  return GeckoMediaPluginService::Init();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aSomeData)
{
  LOGD(("%s::%s topic='%s' data='%s'", __CLASS__, __FUNCTION__,
       aTopic, NS_ConvertUTF16toUTF8(aSomeData).get()));
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch( do_QueryInterface(aSubject) );
    if (branch) {
      bool crashNow = false;
      if (NS_LITERAL_STRING("media.gmp.plugin.crash").Equals(aSomeData)) {
        branch->GetBoolPref("media.gmp.plugin.crash",  &crashNow);
      }
      if (crashNow) {
        nsCOMPtr<nsIThread> gmpThread;
        {
          MutexAutoLock lock(mMutex);
          gmpThread = mGMPThread;
        }
        if (gmpThread) {
          // Note: the GeckoMediaPluginServiceParent singleton is kept alive by a
          // static refptr that is only cleared in the final stage of
          // shutdown after everything else is shutdown, so this RefPtr<> is not
          // strictly necessary so long as that is true, but it's safer.
          gmpThread->Dispatch(WrapRunnable(RefPtr<GeckoMediaPluginServiceParent>(this),
                                           &GeckoMediaPluginServiceParent::CrashPlugins),
                              NS_DISPATCH_NORMAL);
        }
      }
    }
  } else if (!strcmp("profile-change-teardown", aTopic)) {
    mWaitingForPluginsSyncShutdown = true;

    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mShuttingDown);
      mShuttingDown = true;
      gmpThread = mGMPThread;
    }

    if (gmpThread) {
      LOGD(("%s::%s Starting to unload plugins, waiting for sync shutdown..."
            , __CLASS__, __FUNCTION__));
      gmpThread->Dispatch(
        NewRunnableMethod("gmp::GeckoMediaPluginServiceParent::UnloadPlugins",
                          this,
                          &GeckoMediaPluginServiceParent::UnloadPlugins),
        NS_DISPATCH_NORMAL);

      // Wait for UnloadPlugins() to do sync shutdown...
      SpinEventLoopUntil([&]() { return !mWaitingForPluginsSyncShutdown; });
    } else {
      // GMP thread has already shutdown.
      MOZ_ASSERT(mPlugins.IsEmpty());
      mWaitingForPluginsSyncShutdown = false;
    }

  } else if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    MOZ_ASSERT(mShuttingDown);
    ShutdownGMPThread();
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
      return GMPDispatch(
        NewRunnableMethod("gmp::GeckoMediaPluginServiceParent::ClearStorage",
                          this,
                          &GeckoMediaPluginServiceParent::ClearStorage));
    }

    // Clear nodeIds/records modified after |t|.
    nsresult rv;
    PRTime t = nsDependentString(aSomeData).ToInteger64(&rv, 10);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return GMPDispatch(NewRunnableMethod<PRTime>(
      "gmp::GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread",
      this,
      &GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread,
      t));
  }

  return NS_OK;
}

RefPtr<GenericPromise>
GeckoMediaPluginServiceParent::EnsureInitialized() {
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
  GMPCrashHelper* aHelper,
  const nsACString& aNodeIdString,
  const nsCString& aAPI,
  const nsTArray<nsCString>& aTags)
{
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    return GetGMPContentParentPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  typedef MozPromiseHolder<GetGMPContentParentPromise> PromiseHolder;
  PromiseHolder* rawHolder = new PromiseHolder();
  RefPtr<GeckoMediaPluginServiceParent> self(this);
  RefPtr<GetGMPContentParentPromise> promise = rawHolder->Ensure(__func__);
  nsCString nodeIdString(aNodeIdString);
  nsTArray<nsCString> tags(aTags);
  nsCString api(aAPI);
  RefPtr<GMPCrashHelper> helper(aHelper);
  EnsureInitialized()->Then(
    thread,
    __func__,
    [self, tags, api, nodeIdString, helper, rawHolder]() -> void {
      UniquePtr<PromiseHolder> holder(rawHolder);
      RefPtr<GMPParent> gmp = self->SelectPluginForAPI(nodeIdString, api, tags);
      LOGD(("%s: %p returning %p for api %s", __FUNCTION__, (void *)self, (void *)gmp, api.get()));
      if (!gmp) {
        NS_WARNING("GeckoMediaPluginServiceParent::GetContentParentFrom failed");
        holder->Reject(NS_ERROR_FAILURE, __func__);
        return;
      }
      self->ConnectCrashHelper(gmp->GetPluginId(), helper);
      gmp->GetGMPContentParent(Move(holder));
    },
    [rawHolder]() -> void {
      UniquePtr<PromiseHolder> holder(rawHolder);
      NS_WARNING("GMPService::EnsureInitialized failed.");
      holder->Reject(NS_ERROR_FAILURE, __func__);
    });

  return promise;
}

RefPtr<GetGMPContentParentPromise>
GeckoMediaPluginServiceParent::GetContentParent(
  GMPCrashHelper* aHelper,
  const NodeId& aNodeId,
  const nsCString& aAPI,
  const nsTArray<nsCString>& aTags)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  nsCString nodeIdString;
  nsresult rv = GetNodeId(
    aNodeId.mOrigin, aNodeId.mTopLevelOrigin, aNodeId.mGMPName, nodeIdString);
  if (NS_FAILED(rv)) {
    return GetGMPContentParentPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }
  return GetContentParent(aHelper, nodeIdString, aAPI, aTags);
}

void
GeckoMediaPluginServiceParent::InitializePlugins(
  AbstractThread* aAbstractGMPThread)
{
  MOZ_ASSERT(aAbstractGMPThread);
  MonitorAutoLock lock(mInitPromiseMonitor);
  if (mLoadPluginsFromDiskComplete) {
    return;
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  RefPtr<GenericPromise> p = mInitPromise.Ensure(__func__);
  InvokeAsync(aAbstractGMPThread, this, __func__,
              &GeckoMediaPluginServiceParent::LoadFromEnvironment)
    ->Then(aAbstractGMPThread, __func__,
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

void
GeckoMediaPluginServiceParent::NotifySyncShutdownComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  mWaitingForPluginsSyncShutdown = false;
}

bool
GeckoMediaPluginServiceParent::IsShuttingDown()
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  return mShuttingDownOnGMPThread;
}

void
GeckoMediaPluginServiceParent::UnloadPlugins()
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(!mShuttingDownOnGMPThread);
  mShuttingDownOnGMPThread = true;

  nsTArray<RefPtr<GMPParent>> plugins;
  {
    MutexAutoLock lock(mMutex);
    // Move all plugins references to a local array. This way mMutex won't be
    // locked when calling CloseActive (to avoid inter-locking).
    Swap(plugins, mPlugins);

    for (GMPServiceParent* parent : mServiceParents) {
      Unused << parent->SendBeginShutdown();
    }
  }

  LOGD(("%s::%s plugins:%" PRIuSIZE, __CLASS__, __FUNCTION__,
        plugins.Length()));
#ifdef DEBUG
  for (const auto& plugin : plugins) {
    LOGD(("%s::%s plugin: '%s'", __CLASS__, __FUNCTION__,
          plugin->GetDisplayName().get()));
  }
#endif
  // Note: CloseActive may be async; it could actually finish
  // shutting down when all the plugins have unloaded.
  for (const auto& plugin : plugins) {
    plugin->CloseActive(true);
  }

  nsCOMPtr<nsIRunnable> task = NewRunnableMethod(
    "GeckoMediaPluginServiceParent::NotifySyncShutdownComplete",
    this, &GeckoMediaPluginServiceParent::NotifySyncShutdownComplete);
  mMainThread->Dispatch(task.forget());
}

void
GeckoMediaPluginServiceParent::CrashPlugins()
{
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->Crash();
  }
}

RefPtr<GenericPromise::AllPromiseType>
GeckoMediaPluginServiceParent::LoadFromEnvironment()
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    return GenericPromise::AllPromiseType::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  const char* env = PR_GetEnv("MOZ_GMP_PATH");
  if (!env || !*env) {
    return GenericPromise::AllPromiseType::CreateAndResolve(true, __func__);
  }

  nsString allpaths;
  if (NS_WARN_IF(NS_FAILED(NS_CopyNativeToUnicode(nsDependentCString(env), allpaths)))) {
    return GenericPromise::AllPromiseType::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsTArray<RefPtr<GenericPromise>> promises;
  uint32_t pos = 0;
  while (pos < allpaths.Length()) {
    // Loop over multiple path entries separated by colons (*nix) or
    // semicolons (Windows)
    int32_t next = allpaths.FindChar(XPCOM_ENV_PATH_SEPARATOR[0], pos);
    if (next == -1) {
      promises.AppendElement(AddOnGMPThread(nsString(Substring(allpaths, pos))));
      break;
    } else {
      promises.AppendElement(AddOnGMPThread(nsString(Substring(allpaths, pos, next - pos))));
      pos = next + 1;
    }
  }

  mScannedPluginOnDisk = true;
  return GenericPromise::All(thread, promises);
}

class NotifyObserversTask final : public mozilla::Runnable {
public:
  explicit NotifyObserversTask(const char* aTopic, nsString aData = EmptyString())
    : Runnable(aTopic)
    , mTopic(aTopic)
    , mData(aData)
  {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
    MOZ_ASSERT(obsService);
    if (obsService) {
      obsService->NotifyObservers(nullptr, mTopic, mData.get());
    }
    return NS_OK;
  }
private:
  ~NotifyObserversTask() {}
  const char* mTopic;
  const nsString mData;
};

NS_IMETHODIMP
GeckoMediaPluginServiceParent::PathRunnable::Run()
{
  mService->RemoveOnGMPThread(mPath,
                              mOperation == REMOVE_AND_DELETE_FROM_DISK,
                              mDefer);

  mService->UpdateContentProcessGMPCapabilities();
  return NS_OK;
}

void
GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities()
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> task = NewRunnableMethod(
      "GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities",
      this, &GeckoMediaPluginServiceParent::UpdateContentProcessGMPCapabilities);
    mMainThread->Dispatch(task.forget());
    return;
  }

  typedef mozilla::dom::GMPCapabilityData GMPCapabilityData;
  typedef mozilla::dom::GMPAPITags GMPAPITags;
  typedef mozilla::dom::ContentParent ContentParent;

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
      }
      caps.AppendElement(Move(x));
    }
  }
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    Unused << cp->SendGMPsChanged(caps);
  }

  // For non-e10s, we must fire a notification so that any MediaKeySystemAccess
  // requests waiting on a CDM to download will retry.
  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  if (obsService) {
    obsService->NotifyObservers(nullptr, "gmp-changed", nullptr);
  }
}

RefPtr<GenericPromise>
GeckoMediaPluginServiceParent::AsyncAddPluginDirectory(const nsAString& aDirectory)
{
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsString dir(aDirectory);
  RefPtr<GeckoMediaPluginServiceParent> self = this;
  return InvokeAsync(
           thread, this, __func__,
           &GeckoMediaPluginServiceParent::AddOnGMPThread, dir)
    ->Then(
      mMainThread,
      __func__,
      [dir, self](bool aVal) {
        LOGD(("GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s succeeded",
              NS_ConvertUTF16toUTF8(dir).get()));
        MOZ_ASSERT(NS_IsMainThread());
        self->UpdateContentProcessGMPCapabilities();
        return GenericPromise::CreateAndResolve(aVal, __func__);
      },
      [dir](nsresult aResult) {
        LOGD(("GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s failed",
              NS_ConvertUTF16toUTF8(dir).get()));
        return GenericPromise::CreateAndReject(aResult, __func__);
      });
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::AddPluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<GenericPromise> p = AsyncAddPluginDirectory(aDirectory);
  Unused << p;
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::RemovePluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(new PathRunnable(this, aDirectory,
                                      PathRunnable::EOperation::REMOVE));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::RemoveAndDeletePluginDirectory(
  const nsAString& aDirectory, const bool aDefer)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(
    new PathRunnable(this, aDirectory,
                     PathRunnable::EOperation::REMOVE_AND_DELETE_FROM_DISK,
                     aDefer));
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::HasPluginForAPI(const nsACString& aAPI,
                                               nsTArray<nsCString>* aTags,
                                               bool* aHasPlugin)
{
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
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
    RefPtr<GMPParent> gmp = FindPluginForAPIFrom(index, api, *aTags, &index);
    *aHasPlugin = !!gmp;
  }

  return NS_OK;
}

nsresult
GeckoMediaPluginServiceParent::EnsurePluginsOnDiskScanned()
{
  const char* env = nullptr;
  if (!mScannedPluginOnDisk && (env = PR_GetEnv("MOZ_GMP_PATH")) && *env) {
    // We have a MOZ_GMP_PATH environment variable which may specify the
    // location of plugins to load, and we haven't yet scanned the disk to
    // see if there are plugins there. Get the GMP thread, which will
    // cause an event to be dispatched to which scans for plugins. We
    // dispatch a sync event to the GMP thread here in order to wait until
    // after the GMP thread has scanned any paths in MOZ_GMP_PATH.
    nsresult rv = GMPDispatch(new mozilla::Runnable("GMPDummyRunnable"), NS_DISPATCH_SYNC);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(mScannedPluginOnDisk, "Should have scanned MOZ_GMP_PATH by now");
  }

  return NS_OK;
}

already_AddRefed<GMPParent>
GeckoMediaPluginServiceParent::FindPluginForAPIFrom(size_t aSearchStartIndex,
                                                    const nsCString& aAPI,
                                                    const nsTArray<nsCString>& aTags,
                                                    size_t* aOutPluginIndex)
{
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

already_AddRefed<GMPParent>
GeckoMediaPluginServiceParent::SelectPluginForAPI(const nsACString& aNodeId,
                                                  const nsCString& aAPI,
                                                  const nsTArray<nsCString>& aTags)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread(),
             "Can't clone GMP plugins on non-GMP threads.");

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

RefPtr<GMPParent>
CreateGMPParent(AbstractThread* aMainThread)
{
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
  if (!SandboxInfo::Get().CanSandboxMedia()) {
    if (!MediaPrefs::GMPAllowInsecure()) {
      NS_WARNING("Denying media plugin load due to lack of sandboxing.");
      return nullptr;
    }
    NS_WARNING("Loading media plugin despite lack of sandboxing.");
  }
#endif
  return new GMPParent(aMainThread);
}

already_AddRefed<GMPParent>
GeckoMediaPluginServiceParent::ClonePlugin(const GMPParent* aOriginal)
{
  MOZ_ASSERT(aOriginal);

  RefPtr<GMPParent> gmp = CreateGMPParent(mMainThread);
  nsresult rv = gmp ? gmp->CloneFrom(aOriginal) : NS_ERROR_NOT_AVAILABLE;

  if (NS_FAILED(rv)) {
    NS_WARNING("Can't Create GMPParent");
    return nullptr;
  }

  return gmp.forget();
}

RefPtr<GenericPromise>
GeckoMediaPluginServiceParent::AddOnGMPThread(nsString aDirectory)
{
#ifdef XP_WIN
  // On Windows our various test harnesses often pass paths with UNIX dir
  // separators, or a mix of dir separators. NS_NewLocalFile() can't handle
  // that, so fixup to match the platform's expected format. This makes us
  // more robust in the face of bad input and test harnesses changing...
  std::replace(aDirectory.BeginWriting(), aDirectory.EndWriting(), '/', '\\');
#endif

  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  nsCString dir = NS_ConvertUTF16toUTF8(aDirectory);
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    LOGD(("%s::%s: %s No GMP Thread", __CLASS__, __FUNCTION__, dir.get()));
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  LOGD(("%s::%s: %s", __CLASS__, __FUNCTION__, dir.get()));

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOGD(("%s::%s: failed to create nsIFile for dir=%s rv=%" PRIx32,
          __CLASS__, __FUNCTION__, dir.get(), static_cast<uint32_t>(rv)));
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GMPParent> gmp = CreateGMPParent(mMainThread);
  if (!gmp) {
    NS_WARNING("Can't Create GMPParent");
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  return gmp->Init(this, directory)->Then(thread, __func__,
    [gmp, self, dir](bool aVal) {
      LOGD(("%s::%s: %s Succeeded", __CLASS__, __FUNCTION__, dir.get()));
      {
        MutexAutoLock lock(self->mMutex);
        self->mPlugins.AppendElement(gmp);
      }
      return GenericPromise::CreateAndResolve(aVal, __func__);
    },
    [dir](nsresult aResult) {
      LOGD(("%s::%s: %s Failed", __CLASS__, __FUNCTION__, dir.get()));
      return GenericPromise::CreateAndReject(aResult, __func__);
    });
}

void
GeckoMediaPluginServiceParent::RemoveOnGMPThread(const nsAString& aDirectory,
                                                 const bool aDeleteFromDisk,
                                                 const bool aCanDefer)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s: %s", __CLASS__, __FUNCTION__, NS_LossyConvertUTF16toASCII(aDirectory).get()));

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
  for (size_t i = mPlugins.Length(); i-- > 0; ) {
    nsCOMPtr<nsIFile> pluginpath = mPlugins[i]->GetDirectory();
    bool equals;
    if (NS_FAILED(directory->Equals(pluginpath, &equals)) || !equals) {
      continue;
    }

    RefPtr<GMPParent> gmp = mPlugins[i];
    if (aDeleteFromDisk && gmp->State() != GMPStateNotLoaded) {
      // We have to wait for the child process to release its lib handle
      // before we can delete the GMP.
      inUse = true;
      gmp->MarkForDeletion();

      if (!mPluginsWaitingForDeletion.Contains(aDirectory)) {
        mPluginsWaitingForDeletion.AppendElement(aDirectory);
      }
    }

    if (gmp->State() == GMPStateNotLoaded || !aCanDefer) {
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
}

// May remove when Bug 1043671 is fixed
static void Dummy(RefPtr<GMPParent>& aOnDeathsDoor)
{
  // exists solely to do nothing and let the Runnable kill the GMPParent
  // when done.
}

void
GeckoMediaPluginServiceParent::PluginTerminated(const RefPtr<GMPParent>& aPlugin)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());

  if (aPlugin->IsMarkedForDeletion()) {
    nsCString path8;
    RefPtr<nsIFile> dir = aPlugin->GetDirectory();
    nsresult rv = dir->GetNativePath(path8);
    NS_ENSURE_SUCCESS_VOID(rv);

    nsString path = NS_ConvertUTF8toUTF16(path8);
    if (mPluginsWaitingForDeletion.Contains(path)) {
      RemoveOnGMPThread(path, true /* delete */, true /* can defer */);
    }
  }
}

void
GeckoMediaPluginServiceParent::ReAddOnGMPThread(const RefPtr<GMPParent>& aOld)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, (void*) aOld));

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
GeckoMediaPluginServiceParent::GetStorageDir(nsIFile** aOutFile)
{
  if (NS_WARN_IF(!mStorageBaseDir)) {
    return NS_ERROR_FAILURE;
  }
  return mStorageBaseDir->Clone(aOutFile);
}

static nsresult
WriteToFile(nsIFile* aPath,
            const nsCString& aFileName,
            const nsCString& aData)
{
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

  int32_t len = PR_Write(f, aData.get(), aData.Length());
  PR_Close(f);
  if (NS_WARN_IF(len < 0 || (size_t)len != aData.Length())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static nsresult
ReadFromFile(nsIFile* aPath,
             const nsACString& aFileName,
             nsACString& aOutData,
             int32_t aMaxLength)
{
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

nsresult
ReadSalt(nsIFile* aPath, nsACString& aOutData)
{
  return ReadFromFile(aPath, NS_LITERAL_CSTRING("salt"),
                      aOutData, NodeIdSaltLength);

}

already_AddRefed<GMPStorage>
GeckoMediaPluginServiceParent::GetMemoryStorageFor(const nsACString& aNodeId)
{
  RefPtr<GMPStorage> s;
  if (!mTempGMPStorage.Get(aNodeId, getter_AddRefs(s))) {
    s = CreateGMPMemoryStorage();
    mTempGMPStorage.Put(aNodeId, s);
  }
  return s.forget();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::IsPersistentStorageAllowed(const nsACString& aNodeId,
                                                          bool* aOutAllowed)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  NS_ENSURE_ARG(aOutAllowed);
  // We disallow persistent storage for the NodeId used for shared GMP
  // decoding, to prevent GMP decoding being used to track what a user
  // watches somehow.
  *aOutAllowed = !aNodeId.Equals(SHARED_GMP_DECODING_NODE_ID) &&
                 mPersistentStorageAllowed.Get(aNodeId);
  return NS_OK;
}

nsresult
GeckoMediaPluginServiceParent::GetNodeId(const nsAString& aOrigin,
                                         const nsAString& aTopLevelOrigin,
                                         const nsAString& aGMPName,
                                         nsACString& aOutId)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s: (%s, %s)", __CLASS__, __FUNCTION__,
       NS_ConvertUTF16toUTF8(aOrigin).get(),
       NS_ConvertUTF16toUTF8(aTopLevelOrigin).get()));

  nsresult rv;

  if (aOrigin.EqualsLiteral("null") ||
      aOrigin.IsEmpty() ||
      aTopLevelOrigin.EqualsLiteral("null") ||
      aTopLevelOrigin.IsEmpty()) {
    // (origin, topLevelOrigin) is null or empty; this is for an anonymous
    // origin, probably a local file, for which we don't provide persistent storage.
    // Generate a random node id, and don't store it so that the GMP's storage
    // is temporary and the process for this GMP is not shared with GMP
    // instances that have the same nodeId.
    nsAutoCString salt;
    rv = GenerateRandomPathName(salt, NodeIdSaltLength);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aOutId = salt;
    mPersistentStorageAllowed.Put(salt, false);
    return NS_OK;
  }

  const uint32_t hash = AddToHash(HashString(aOrigin),
                                  HashString(aTopLevelOrigin));

  if (OriginAttributes::IsPrivateBrowsing(NS_ConvertUTF16toUTF8(aOrigin))) {
    // For PB mode, we store the node id, indexed by the origin pair and GMP name,
    // so that if the same origin pair is opened for the same GMP in this session,
    // it gets the same node id.
    const uint32_t pbHash = AddToHash(HashString(aGMPName), hash);
    nsCString* salt = nullptr;
    if (!(salt = mTempNodeIds.Get(pbHash))) {
      // No salt stored, generate and temporarily store some for this id.
      nsAutoCString newSalt;
      rv = GenerateRandomPathName(newSalt, NodeIdSaltLength);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      salt = new nsCString(newSalt);
      mTempNodeIds.Put(pbHash, salt);
      mPersistentStorageAllowed.Put(*salt, false);
    }
    aOutId = *salt;
    return NS_OK;
  }

  // Otherwise, try to see if we've previously generated and stored salt
  // for this origin pair.
  nsCOMPtr<nsIFile> path; // $profileDir/gmp/$platform/
  rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->Append(aGMPName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // $profileDir/gmp/$platform/$gmpName/
  rv = path->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->AppendNative(NS_LITERAL_CSTRING("id"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // $profileDir/gmp/$platform/$gmpName/id/
  rv = path->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
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

  rv = saltFile->AppendNative(NS_LITERAL_CSTRING("salt"));
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
    rv = WriteToFile(path, NS_LITERAL_CSTRING("salt"), salt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/$platform/$gmpName/id/$hash/origin
    rv = WriteToFile(path,
                     NS_LITERAL_CSTRING("origin"),
                     NS_ConvertUTF16toUTF8(aOrigin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/$platform/$gmpName/id/$hash/topLevelOrigin
    rv = WriteToFile(path,
                     NS_LITERAL_CSTRING("topLevelOrigin"),
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
  mPersistentStorageAllowed.Put(salt, true);

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetNodeId(const nsAString& aOrigin,
                                         const nsAString& aTopLevelOrigin,
                                         const nsAString& aGMPName,
                                         UniquePtr<GetNodeIdCallback>&& aCallback)
{
  nsCString nodeId;
  nsresult rv = GetNodeId(aOrigin, aTopLevelOrigin, aGMPName, nodeId);
  aCallback->Done(rv, nodeId);
  return rv;
}

static bool
ExtractHostName(const nsACString& aOrigin, nsACString& aOutData)
{
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

bool
MatchOrigin(nsIFile* aPath,
            const nsACString& aSite,
            const mozilla::OriginAttributesPattern& aPattern)
{
  // http://en.wikipedia.org/wiki/Domain_Name_System#Domain_name_syntax
  static const uint32_t MaxDomainLength = 253;

  nsresult rv;
  nsCString str;
  nsCString originNoSuffix;
  mozilla::OriginAttributes originAttributes;

  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("origin"), str, MaxDomainLength);
  if (!originAttributes.PopulateFromOrigin(str, originNoSuffix)) {
    // Fails on parsing the originAttributes, treat this as a non-match.
    return false;
  }

  if (NS_SUCCEEDED(rv) && ExtractHostName(originNoSuffix, str) && str.Equals(aSite) &&
      aPattern.Matches(originAttributes)) {
    return true;
  }

  mozilla::OriginAttributes topLevelOriginAttributes;
  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("topLevelOrigin"), str, MaxDomainLength);
  if (!topLevelOriginAttributes.PopulateFromOrigin(str, originNoSuffix)) {
    // Fails on paring the originAttributes, treat this as a non-match.
    return false;
  }

  if (NS_SUCCEEDED(rv) && ExtractHostName(originNoSuffix, str) && str.Equals(aSite) &&
      aPattern.Matches(topLevelOriginAttributes)) {
    return true;
  }
  return false;
}

template<typename T> static void
KillPlugins(const nsTArray<RefPtr<GMPParent>>& aPlugins,
            Mutex& aMutex, T&& aFilter)
{
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

static nsresult
DeleteDir(nsIFile* aPath)
{
  bool exists = false;
  nsresult rv = aPath->Exists(&exists);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (exists) {
    return aPath->Remove(true);
  }
  return NS_OK;
}

struct NodeFilter {
  explicit NodeFilter(const nsTArray<nsCString>& nodeIDs) : mNodeIDs(nodeIDs) {}
  bool operator()(GMPParent* aParent) {
    return mNodeIDs.Contains(aParent->GetNodeId());
  }
private:
  const nsTArray<nsCString>& mNodeIDs;
};

void
GeckoMediaPluginServiceParent::ClearNodeIdAndPlugin(DirectoryFilter& aFilter)
{
  // $profileDir/gmp/$platform/
  nsCOMPtr<nsIFile> path;
  nsresult rv = GetStorageDir(getter_AddRefs(path));
  if (NS_FAILED(rv)) {
    return;
  }

  // Iterate all sub-folders of $profileDir/gmp/$platform/, i.e. the dirs in which
  // specific GMPs store their data.
  DirectoryEnumerator iter(path, DirectoryEnumerator::DirsOnly);
  for (nsCOMPtr<nsIFile> pluginDir; (pluginDir = iter.Next()) != nullptr;) {
    ClearNodeIdAndPlugin(pluginDir, aFilter);
  }
}

void
GeckoMediaPluginServiceParent::ClearNodeIdAndPlugin(nsIFile* aPluginStorageDir,
                                                    DirectoryFilter& aFilter)
{
  // $profileDir/gmp/$platform/$gmpName/id/
  nsCOMPtr<nsIFile> path = CloneAndAppend(aPluginStorageDir, NS_LITERAL_STRING("id"));
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
  KillPlugins(mPlugins, mMutex, NodeFilter(nodeIDsToClear));

  // Clear all storage in $profileDir/gmp/$platform/$gmpName/storage/$nodeId/
  path = CloneAndAppend(aPluginStorageDir, NS_LITERAL_STRING("storage"));
  if (!path) {
    return;
  }

  for (const nsCString& nodeId : nodeIDsToClear) {
    nsCOMPtr<nsIFile> dirEntry;
    nsresult rv = path->Clone(getter_AddRefs(dirEntry));
    if (NS_FAILED(rv)) {
      continue;
    }

    rv = dirEntry->AppendNative(nodeId);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (NS_FAILED(DeleteDir(dirEntry))) {
      NS_WARNING("Failed to delete GMP storage directory for the node");
    }
  }
}

void
GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread(const nsACString& aSite,
                                                         const mozilla::OriginAttributesPattern& aPattern)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s: origin=%s", __CLASS__, __FUNCTION__, aSite.Data()));

  struct OriginFilter : public DirectoryFilter {
    explicit OriginFilter(const nsACString& aSite,
                          const mozilla::OriginAttributesPattern& aPattern)
    : mSite(aSite)
    , mPattern(aPattern)
    { }
    bool operator()(nsIFile* aPath) override {
      return MatchOrigin(aPath, mSite, mPattern);
    }
  private:
    const nsACString& mSite;
    const mozilla::OriginAttributesPattern& mPattern;
  } filter(aSite, aPattern);

  ClearNodeIdAndPlugin(filter);
}

void
GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread(PRTime aSince)
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s: since=%" PRId64, __CLASS__, __FUNCTION__, (int64_t)aSince));

  struct MTimeFilter : public DirectoryFilter {
    explicit MTimeFilter(PRTime aSince)
      : mSince(aSince) {}

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
      if (NS_FAILED(ReadSalt(aPath, salt))) {
        return false;
      }

      // $profileDir/gmp/$platform/$gmpName/id/
      nsCOMPtr<nsIFile> idDir;
      if (NS_FAILED(aPath->GetParent(getter_AddRefs(idDir)))) {
        return false;
      }
      // $profileDir/gmp/$platform/$gmpName/
      nsCOMPtr<nsIFile> temp;
      if (NS_FAILED(idDir->GetParent(getter_AddRefs(temp)))) {
        return false;
      }

      // $profileDir/gmp/$platform/$gmpName/storage/
      if (NS_FAILED(temp->Append(NS_LITERAL_STRING("storage")))) {
        return false;
      }
      // $profileDir/gmp/$platform/$gmpName/storage/$originSalt
      return NS_SUCCEEDED(temp->AppendNative(salt)) && IsModifiedAfter(temp);
    }
  private:
    const PRTime mSince;
  } filter(aSince);

  ClearNodeIdAndPlugin(filter);

  nsCOMPtr<nsIRunnable> task
    = new NotifyObserversTask("gmp-clear-storage-complete");
  mMainThread->Dispatch(task.forget());
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::ForgetThisSite(const nsAString& aSite,
                                              const nsAString& aPattern)
{
  MOZ_ASSERT(NS_IsMainThread());

  mozilla::OriginAttributesPattern pattern;

  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  return ForgetThisSiteNative(aSite, pattern);
}

nsresult
GeckoMediaPluginServiceParent::ForgetThisSiteNative(const nsAString& aSite,
                                                    const mozilla::OriginAttributesPattern& aPattern)
{
  MOZ_ASSERT(NS_IsMainThread());

  return GMPDispatch(
    NewRunnableMethod<nsCString, mozilla::OriginAttributesPattern>(
      "gmp::GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread",
      this,
      &GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread,
      NS_ConvertUTF16toUTF8(aSite),
      aPattern));
}

static bool IsNodeIdValid(GMPParent* aParent) {
  return !aParent->GetNodeId().IsEmpty();
}

static nsCOMPtr<nsIAsyncShutdownClient>
GetShutdownBarrier()
{
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetXpcomWillShutdown(getter_AddRefs(barrier));

  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(barrier);
  return barrier.forget();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetName(nsAString& aName)
{
  aName = NS_LITERAL_STRING("GeckoMediaPluginServiceParent: shutdown");
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::GetState(nsIPropertyBag**)
{
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::BlockShutdown(nsIAsyncShutdownClient*)
{
  return NS_OK;
}

void
GeckoMediaPluginServiceParent::ServiceUserCreated(
  GMPServiceParent* aServiceParent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mServiceParents.Contains(aServiceParent));
  mServiceParents.AppendElement(aServiceParent);
  if (mServiceParents.Length() == 1) {
    nsresult rv = GetShutdownBarrier()->AddBlocker(
      this, NS_LITERAL_STRING(__FILE__), __LINE__,
      NS_LITERAL_STRING("GeckoMediaPluginServiceParent shutdown"));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
}

void
GeckoMediaPluginServiceParent::ServiceUserDestroyed(
  GMPServiceParent* aServiceParent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mServiceParents.Length() > 0);
  MOZ_ASSERT(mServiceParents.Contains(aServiceParent));
  mServiceParents.RemoveElement(aServiceParent);
  if (mServiceParents.IsEmpty()) {
    nsresult rv = GetShutdownBarrier()->RemoveBlocker(this);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  }
}

void
GeckoMediaPluginServiceParent::ClearStorage()
{
  MOZ_ASSERT(mGMPThread->EventTarget()->IsOnCurrentThread());
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));

  // Kill plugins with valid nodeIDs.
  KillPlugins(mPlugins, mMutex, &IsNodeIdValid);

  nsCOMPtr<nsIFile> path; // $profileDir/gmp/$platform/
  nsresult rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_FAILED(DeleteDir(path))) {
    NS_WARNING("Failed to delete GMP storage directory");
  }

  // Clear private-browsing storage.
  mTempGMPStorage.Clear();

  nsCOMPtr<nsIRunnable> task
    = new NotifyObserversTask("gmp-clear-storage-complete");
  mMainThread->Dispatch(task.forget());
}

already_AddRefed<GMPParent>
GeckoMediaPluginServiceParent::GetById(uint32_t aPluginId)
{
  MutexAutoLock lock(mMutex);
  for (const RefPtr<GMPParent>& gmp : mPlugins) {
    if (gmp->GetPluginId() == aPluginId) {
      return do_AddRef(gmp);
    }
  }
  return nullptr;
}

GMPServiceParent::GMPServiceParent(GeckoMediaPluginServiceParent* aService)
  : mService(aService)
{
  MOZ_ASSERT(mService);
  mService->ServiceUserCreated(this);
}

GMPServiceParent::~GMPServiceParent()
{
  MOZ_ASSERT(mService);
  mService->ServiceUserDestroyed(this);
}

mozilla::ipc::IPCResult
GMPServiceParent::RecvLaunchGMP(const nsCString& aNodeId,
                                const nsCString& aAPI,
                                nsTArray<nsCString>&& aTags,
                                nsTArray<ProcessId>&& aAlreadyBridgedTo,
                                uint32_t* aOutPluginId,
                                ProcessId* aOutProcessId,
                                nsCString* aOutDisplayName,
                                Endpoint<PGMPContentParent>* aOutEndpoint,
                                nsresult* aOutRv)
{
  if (mService->IsShuttingDown()) {
    *aOutRv = NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    return IPC_OK();
  }

  RefPtr<GMPParent> gmp = mService->SelectPluginForAPI(aNodeId, aAPI, aTags);
  if (gmp) {
    *aOutPluginId = gmp->GetPluginId();
  } else {
    *aOutRv = NS_ERROR_FAILURE;
    *aOutPluginId = 0;
    return IPC_OK();
  }

  if (!gmp->EnsureProcessLoaded(aOutProcessId)) {
    *aOutRv = NS_ERROR_FAILURE;
    return IPC_OK();
  }

  *aOutDisplayName = gmp->GetDisplayName();

  if (aAlreadyBridgedTo.Contains(*aOutProcessId)) {
    *aOutRv = NS_OK;
    return IPC_OK();
  }

  Endpoint<PGMPContentParent> parent;
  Endpoint<PGMPContentChild> child;
  if (NS_FAILED(PGMPContent::CreateEndpoints(OtherPid(), *aOutProcessId,
                                             &parent, &child))) {
    *aOutRv = NS_ERROR_FAILURE;
    return IPC_OK();
  }

  *aOutEndpoint = Move(parent);

  if (!gmp->SendInitGMPContentChild(Move(child))) {
    *aOutRv = NS_ERROR_FAILURE;
    return IPC_OK();
  }

  gmp->IncrementGMPContentChildCount();

  *aOutRv = NS_OK;
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPServiceParent::RecvLaunchGMPForNodeId(
  const NodeIdData& aNodeId,
  const nsCString& aApi,
  nsTArray<nsCString>&& aTags,
  nsTArray<ProcessId>&& aAlreadyBridgedTo,
  uint32_t* aOutPluginId,
  ProcessId* aOutId,
  nsCString* aOutDisplayName,
  Endpoint<PGMPContentParent>* aOutEndpoint,
  nsresult* aOutRv)
{
  nsCString nodeId;
  nsresult rv = mService->GetNodeId(
    aNodeId.mOrigin(), aNodeId.mTopLevelOrigin(), aNodeId.mGMPName(), nodeId);
  if (!NS_SUCCEEDED(rv)) {
    *aOutRv = rv;
    return IPC_OK();
  }
  return RecvLaunchGMP(nodeId,
                       aApi,
                       Move(aTags),
                       Move(aAlreadyBridgedTo),
                       aOutPluginId,
                       aOutId,
                       aOutDisplayName,
                       aOutEndpoint,
                       aOutRv);
}

mozilla::ipc::IPCResult
GMPServiceParent::RecvGetGMPNodeId(const nsString& aOrigin,
                                   const nsString& aTopLevelOrigin,
                                   const nsString& aGMPName,
                                   nsCString* aID)
{
  nsresult rv = mService->GetNodeId(aOrigin, aTopLevelOrigin, aGMPName, *aID);
  if (!NS_SUCCEEDED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

class DeleteGMPServiceParent : public mozilla::Runnable
{
public:
  explicit DeleteGMPServiceParent(GMPServiceParent* aToDelete)
    : Runnable("gmp::DeleteGMPServiceParent")
    , mToDelete(aToDelete)
  {
  }

  NS_IMETHOD Run() override
  {
    return NS_OK;
  }

private:
  nsAutoPtr<GMPServiceParent> mToDelete;
};

void GMPServiceParent::CloseTransport(Monitor* aSyncMonitor, bool* aCompleted)
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  MonitorAutoLock lock(*aSyncMonitor);

  // This deletes the transport.
  SetTransport(nullptr);

  *aCompleted = true;
  lock.NotifyAll();
}

void
GMPServiceParent::ActorDestroy(ActorDestroyReason aWhy)
{
  Monitor monitor("DeleteGMPServiceParent");
  bool completed = false;

  // Make sure the IPC channel is closed before destroying mToDelete.
  MonitorAutoLock lock(monitor);
  RefPtr<Runnable> task = NewNonOwningRunnableMethod<Monitor*, bool*>(
    "gmp::GMPServiceParent::CloseTransport",
    this,
    &GMPServiceParent::CloseTransport,
    &monitor,
    &completed);
  XRE_GetIOMessageLoop()->PostTask(Move(task.forget()));

  while (!completed) {
    lock.Wait();
  }

  // Dispatch a task to the current thread to ensure we don't delete the
  // GMPServiceParent until the current calling context is finished with
  // the object.
  GMPServiceParent* self = this;
  NS_DispatchToCurrentThread(
    NS_NewRunnableFunction("gmp::GMPServiceParent::ActorDestroy", [self]() {
      // The GMPServiceParent must be destroyed on the main thread.
      NS_DispatchToMainThread(NS_NewRunnableFunction(
        "gmp::GMPServiceParent::ActorDestroy", [self]() { delete self; }));
    }));
}

class OpenPGMPServiceParent : public mozilla::Runnable
{
public:
  OpenPGMPServiceParent(GMPServiceParent* aGMPServiceParent,
                        ipc::Endpoint<PGMPServiceParent>&& aEndpoint,
                        bool* aResult)
    : Runnable("gmp::OpenPGMPServiceParent")
    , mGMPServiceParent(aGMPServiceParent)
    , mEndpoint(Move(aEndpoint))
    , mResult(aResult)
  {
  }

  NS_IMETHOD Run() override
  {
    *mResult = mEndpoint.Bind(mGMPServiceParent);
    return NS_OK;
  }

private:
  GMPServiceParent* mGMPServiceParent;
  ipc::Endpoint<PGMPServiceParent> mEndpoint;
  bool* mResult;
};

/* static */
bool
GMPServiceParent::Create(Endpoint<PGMPServiceParent>&& aGMPService)
{
  RefPtr<GeckoMediaPluginServiceParent> gmp =
    GeckoMediaPluginServiceParent::GetSingleton();

  if (gmp->mShuttingDown) {
    // Shutdown is initiated. There is no point creating a new actor.
    return false;
  }

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = gmp->GetThread(getter_AddRefs(gmpThread));
  NS_ENSURE_SUCCESS(rv, false);

  nsAutoPtr<GMPServiceParent> serviceParent(new GMPServiceParent(gmp));
  bool ok;
  rv = gmpThread->Dispatch(new OpenPGMPServiceParent(serviceParent,
                                                     Move(aGMPService),
                                                     &ok),
                           NS_DISPATCH_SYNC);

  if (NS_FAILED(rv) || !ok) {
    return false;
  }

  // Now that the service parent is set up, it will be destroyed by
  // ActorDestroy.
  Unused << serviceParent.forget();

  return true;
}

} // namespace gmp
} // namespace mozilla

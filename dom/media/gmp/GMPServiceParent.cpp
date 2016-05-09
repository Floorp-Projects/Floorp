/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceParent.h"
#include "GMPService.h"
#include "prio.h"
#include "mozilla/Logging.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "nsIObserverService.h"
#include "GeckoChildProcessHost.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/Services.h"
#include "nsNativeCharsetUtils.h"
#include "nsIConsoleService.h"
#include "mozilla/unused.h"
#include "GMPDecryptorParent.h"
#include "GMPAudioDecoderParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Preferences.h"
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
                            mozIGeckoMediaPluginChromeService)

static int32_t sMaxAsyncShutdownWaitMs = 0;
static bool sAllowInsecureGMP = false;
static bool sHaveSetGMPServiceParentPrefCaches = false;

GeckoMediaPluginServiceParent::GeckoMediaPluginServiceParent()
  : mShuttingDown(false)
#ifdef MOZ_CRASHREPORTER
  , mAsyncShutdownPluginStatesMutex("GeckoMediaPluginService::mAsyncShutdownPluginStatesMutex")
#endif
  , mScannedPluginOnDisk(false)
  , mWaitingForPluginsSyncShutdown(false)
  , mInitPromiseMonitor("GeckoMediaPluginServiceParent::mInitPromiseMonitor")
  , mLoadPluginsFromDiskComplete(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sHaveSetGMPServiceParentPrefCaches) {
    sHaveSetGMPServiceParentPrefCaches = true;
    Preferences::AddIntVarCache(&sMaxAsyncShutdownWaitMs,
                                "media.gmp.async-shutdown-timeout",
                                GMP_DEFAULT_ASYNC_SHUTDONW_TIMEOUT);
    Preferences::AddBoolVarCache(&sAllowInsecureGMP,
                                 "media.gmp.insecure.allow", false);
  }
  mInitPromise.SetMonitor(&mInitPromiseMonitor);
}

GeckoMediaPluginServiceParent::~GeckoMediaPluginServiceParent()
{
  MOZ_ASSERT(mPlugins.IsEmpty());
  MOZ_ASSERT(mAsyncShutdownPlugins.IsEmpty());
}

int32_t
GeckoMediaPluginServiceParent::AsyncShutdownTimeoutMs()
{
  MOZ_ASSERT(sHaveSetGMPServiceParentPrefCaches);
  return sMaxAsyncShutdownWaitMs;
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
    return GMPDispatch(NewRunnableMethod(
      this, &GeckoMediaPluginServiceParent::ClearStorage));
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

static void
MoveAndOverwrite(nsIFile* aOldParentDir,
                 nsIFile* aNewParentDir,
                 const nsAString& aSubDir)
{
  nsresult rv;

  nsCOMPtr<nsIFile> srcDir(CloneAndAppend(aOldParentDir, aSubDir));
  if (NS_WARN_IF(!srcDir)) {
    return;
  }

  if (!FileExists(srcDir)) {
    // No sub-directory to be migrated.
    return;
  }

  // Ensure destination parent directory exists.
  rv = aNewParentDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIFile> dstDir(CloneAndAppend(aNewParentDir, aSubDir));
  if (FileExists(dstDir)) {
    // We must have migrated before already, and then ran an old version
    // of Gecko again which created storage at the old location. Overwrite
    // the previously migrated storage.
    rv = dstDir->Remove(true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // MoveTo will fail.
      return;
    }
  }

  rv = srcDir->MoveTo(aNewParentDir, EmptyString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

static void
MigratePreGecko42StorageDir(nsIFile* aOldStorageDir,
                            nsIFile* aNewStorageDir)
{
  MoveAndOverwrite(aOldStorageDir, aNewStorageDir, NS_LITERAL_STRING("id"));
  MoveAndOverwrite(aOldStorageDir, aNewStorageDir, NS_LITERAL_STRING("storage"));
}

static void
MigratePreGecko45StorageDir(nsIFile* aStorageDirBase)
{
  nsCOMPtr<nsIFile> adobeStorageDir(CloneAndAppend(aStorageDirBase, NS_LITERAL_STRING("gmp-eme-adobe")));
  if (NS_WARN_IF(!adobeStorageDir)) {
    return;
  }

  // The base storage dir in pre-45 contained "id" and "storage" subdirs.
  // We assume all storage in the base storage dir that aren't known to GMP
  // storage are records for the Adobe GMP.
  MoveAndOverwrite(aStorageDirBase, adobeStorageDir, NS_LITERAL_STRING("id"));
  MoveAndOverwrite(aStorageDirBase, adobeStorageDir, NS_LITERAL_STRING("storage"));
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

  // Prior to 42, GMP storage was stored in $profileDir/gmp/. After 42, it's
  // stored in $profileDir/gmp/$platform/. So we must migrate any old records
  // from the old location to the new location, for forwards compatibility.
  MigratePreGecko42StorageDir(gmpDirWithoutPlatform, mStorageBaseDir);

  // Prior to 45, GMP storage was not separated by plugin. In 45 and after,
  // it's stored in $profile/gmp/$platform/$gmpName. So we must migrate old
  // records from the old location to the new location, for forwards
  // compatibility. We assume all directories in the base storage dir that
  // aren't known to GMP storage are records for the Adobe GMP, since it
  // was first.
  MigratePreGecko45StorageDir(mStorageBaseDir);

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
          gmpThread->Dispatch(WrapRunnable(this,
                                           &GeckoMediaPluginServiceParent::CrashPlugins),
                              NS_DISPATCH_NORMAL);
        }
      }
    }
  } else if (!strcmp("profile-change-teardown", aTopic)) {

    // How shutdown works:
    //
    // Some GMPs require time to do bookkeeping upon shutdown. These GMPs
    // need to be given time to access storage during shutdown. To signal
    // that time to shutdown is required, those GMPs implement the
    // GMPAsyncShutdown interface.
    //
    // When we startup the child process, we query the GMP for the
    // GMPAsyncShutdown interface, and if it's present, we send a message
    // back to the GMPParent, which then registers the GMPParent by calling
    // GMPService::AsyncShutdownNeeded().
    //
    // On shutdown, we set mWaitingForPluginsSyncShutdown to true, and then
    // call UnloadPlugins on the GMPThread, and process events on the main
    // thread until 1. An event sets mWaitingForPluginsSyncShutdown=false on
    // the main thread; then 2. All async-shutdown plugins have indicated
    // they have completed shutdown.
    //
    // UnloadPlugins() sends close messages for all plugins' API objects to
    // the GMP interfaces in the child process, and then sends the async
    // shutdown notifications to child GMPs. When a GMP has completed its
    // shutdown, it calls GMPAsyncShutdownHost::ShutdownComplete(), which
    // sends a message back to the parent, which calls
    // GMPService::AsyncShutdownComplete(). If all plugins requiring async
    // shutdown have called AsyncShutdownComplete() we stick a dummy event on
    // the main thread, where the list of pending plugins is checked. We must
    // use an event to do this, as we must ensure the main thread processes an
    // event to run its loop. This will unblock the main thread, and shutdown
    // of other components will proceed.
    //
    // During shutdown, each GMPParent starts a timer, and pretends shutdown
    // is complete if it is taking too long.
    //
    // We shutdown in "profile-change-teardown", as the profile dir is
    // still writable then, and it's required for GMPStorage. We block the
    // shutdown process by spinning the main thread event loop until all GMPs
    // have shutdown, or timeout has occurred.
    //
    // GMPStorage needs to work up until the shutdown-complete notification
    // arrives from the GMP process.

    mWaitingForPluginsSyncShutdown = true;

    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mShuttingDown);
      mShuttingDown = true;
      gmpThread = mGMPThread;
    }

    if (gmpThread) {
      LOGD(("%s::%s Starting to unload plugins, waiting for first sync shutdown..."
            , __CLASS__, __FUNCTION__));
#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '0',
        NS_LITERAL_CSTRING("Dispatching UnloadPlugins"));
#endif
      gmpThread->Dispatch(
        NewRunnableMethod(this,
                          &GeckoMediaPluginServiceParent::UnloadPlugins),
        NS_DISPATCH_NORMAL);

#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '1',
        NS_LITERAL_CSTRING("Waiting for sync shutdown"));
#endif
      // Wait for UnloadPlugins() to do initial sync shutdown...
      while (mWaitingForPluginsSyncShutdown) {
        NS_ProcessNextEvent(NS_GetCurrentThread(), true);
      }

#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '4',
        NS_LITERAL_CSTRING("Waiting for async shutdown"));
#endif
      // Wait for other plugins (if any) to do async shutdown...
      auto syncShutdownPluginsRemaining =
        std::numeric_limits<decltype(mAsyncShutdownPlugins.Length())>::max();
      for (;;) {
        {
          MutexAutoLock lock(mMutex);
          if (mAsyncShutdownPlugins.IsEmpty()) {
            LOGD(("%s::%s Finished unloading all plugins"
                  , __CLASS__, __FUNCTION__));
#if defined(MOZ_CRASHREPORTER)
            CrashReporter::RemoveCrashReportAnnotation(
              NS_LITERAL_CSTRING("AsyncPluginShutdown"));
#endif
            break;
          } else if (mAsyncShutdownPlugins.Length() < syncShutdownPluginsRemaining) {
            // First time here, or number of pending plugins has decreased.
            // -> Update list of pending plugins in crash report.
            syncShutdownPluginsRemaining = mAsyncShutdownPlugins.Length();
            LOGD(("%s::%s Still waiting for %d plugins to shutdown..."
                  , __CLASS__, __FUNCTION__, (int)syncShutdownPluginsRemaining));
#if defined(MOZ_CRASHREPORTER)
            nsAutoCString names;
            for (const auto& plugin : mAsyncShutdownPlugins) {
              if (!names.IsEmpty()) { names.Append(NS_LITERAL_CSTRING(", ")); }
              names.Append(plugin->GetDisplayName());
            }
            CrashReporter::AnnotateCrashReport(
              NS_LITERAL_CSTRING("AsyncPluginShutdown"),
              names);
#endif
          }
        }
        NS_ProcessNextEvent(NS_GetCurrentThread(), true);
      }
#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '5',
        NS_LITERAL_CSTRING("Async shutdown complete"));
#endif
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
      return GMPDispatch(NewRunnableMethod(
          this, &GeckoMediaPluginServiceParent::ClearStorage));
    }

    // Clear nodeIds/records modified after |t|.
    nsresult rv;
    PRTime t = nsDependentString(aSomeData).ToInteger64(&rv, 10);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return GMPDispatch(NewRunnableMethod<PRTime>(
        this, &GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread,
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

bool
GeckoMediaPluginServiceParent::GetContentParentFrom(const nsACString& aNodeId,
                                                    const nsCString& aAPI,
                                                    const nsTArray<nsCString>& aTags,
                                                    UniquePtr<GetGMPContentParentCallback>&& aCallback)
{
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    return false;
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  nsCString nodeId(aNodeId);
  nsTArray<nsCString> tags(aTags);
  nsCString api(aAPI);
  GetGMPContentParentCallback* rawCallback = aCallback.release();
  EnsureInitialized()->Then(thread, __func__,
    [self, tags, api, nodeId, rawCallback]() -> void {
      UniquePtr<GetGMPContentParentCallback> callback(rawCallback);
      RefPtr<GMPParent> gmp = self->SelectPluginForAPI(nodeId, api, tags);
      LOGD(("%s: %p returning %p for api %s", __FUNCTION__, (void *)self, (void *)gmp, api.get()));
      if (!gmp) {
        NS_WARNING("GeckoMediaPluginServiceParent::GetContentParentFrom failed");
        callback->Done(nullptr);
        return;
      }
      gmp->GetGMPContentParent(Move(callback));
    },
    [rawCallback]() -> void {
      UniquePtr<GetGMPContentParentCallback> callback(rawCallback);
      NS_WARNING("GMPService::EnsureInitialized failed.");
      callback->Done(nullptr);
    });
  return true;
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
GeckoMediaPluginServiceParent::AsyncShutdownNeeded(GMPParent* aParent)
{
  LOGD(("%s::%s %p", __CLASS__, __FUNCTION__, aParent));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mAsyncShutdownPlugins.Contains(aParent));
  mAsyncShutdownPlugins.AppendElement(aParent);
}

void
GeckoMediaPluginServiceParent::AsyncShutdownComplete(GMPParent* aParent)
{
  LOGD(("%s::%s %p '%s'", __CLASS__, __FUNCTION__,
        aParent, aParent->GetDisplayName().get()));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  {
    MutexAutoLock lock(mMutex);
    mAsyncShutdownPlugins.RemoveElement(aParent);
  }

  if (mShuttingDownOnGMPThread) {
    // The main thread may be waiting for async shutdown of plugins,
    // one of which has completed. Wake up the main thread by sending a task.
    nsCOMPtr<nsIRunnable> task(NewRunnableMethod(
      this, &GeckoMediaPluginServiceParent::NotifyAsyncShutdownComplete));
    NS_DispatchToMainThread(task);
  }
}

#ifdef MOZ_CRASHREPORTER
void
GeckoMediaPluginServiceParent::SetAsyncShutdownPluginState(GMPParent* aGMPParent,
                                                           char aId,
                                                           const nsCString& aState)
{
  MutexAutoLock lock(mAsyncShutdownPluginStatesMutex);
  if (!aGMPParent) {
    mAsyncShutdownPluginStates.Update(NS_LITERAL_CSTRING("-"),
                                      NS_LITERAL_CSTRING("-"),
                                      aId,
                                      aState);
    return;
  }
  mAsyncShutdownPluginStates.Update(aGMPParent->GetDisplayName(),
                                    nsPrintfCString("%p", aGMPParent),
                                    aId,
                                    aState);
}

void
GeckoMediaPluginServiceParent::AsyncShutdownPluginStates::Update(const nsCString& aPlugin,
                                                                 const nsCString& aInstance,
                                                                 char aId,
                                                                 const nsCString& aState)
{
  nsCString note;
  StatesByInstance* instances = mStates.LookupOrAdd(aPlugin);
  if (!instances) { return; }
  State* state = instances->LookupOrAdd(aInstance);
  if (!state) { return; }
  state->mStateSequence += aId;
  state->mLastStateDescription = aState;
  note += '{';
  bool firstPlugin = true;
  for (auto pluginIt = mStates.ConstIter(); !pluginIt.Done(); pluginIt.Next()) {
    if (!firstPlugin) { note += ','; } else { firstPlugin = false; }
    note += pluginIt.Key();
    note += ":{";
    bool firstInstance = true;
    for (auto instanceIt = pluginIt.UserData()->ConstIter(); !instanceIt.Done(); instanceIt.Next()) {
      if (!firstInstance) { note += ','; } else { firstInstance = false; }
      note += instanceIt.Key();
      note += ":\"";
      note += instanceIt.UserData()->mStateSequence;
      note += '=';
      note += instanceIt.UserData()->mLastStateDescription;
      note += '"';
    }
    note += '}';
  }
  note += '}';
  LOGD(("%s::%s states[%s][%s]='%c'/'%s' -> %s", __CLASS__, __FUNCTION__,
        aPlugin.get(), aInstance.get(), aId, aState.get(), note.get()));
  CrashReporter::AnnotateCrashReport(
    NS_LITERAL_CSTRING("AsyncPluginShutdownStates"),
    note);
}
#endif // MOZ_CRASHREPORTER

void
GeckoMediaPluginServiceParent::NotifyAsyncShutdownComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Nothing to do, this task is just used to wake up the event loop in Observe().
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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  return mShuttingDownOnGMPThread;
}

void
GeckoMediaPluginServiceParent::UnloadPlugins()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  MOZ_ASSERT(!mShuttingDownOnGMPThread);
  mShuttingDownOnGMPThread = true;
#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '2',
        NS_LITERAL_CSTRING("Starting to unload plugins"));
#endif

  nsTArray<RefPtr<GMPParent>> plugins;
  {
    MutexAutoLock lock(mMutex);
    // Move all plugins references to a local array. This way mMutex won't be
    // locked when calling CloseActive (to avoid inter-locking).
    plugins = Move(mPlugins);
  }

  LOGD(("%s::%s plugins:%u including async:%u", __CLASS__, __FUNCTION__,
        plugins.Length(), mAsyncShutdownPlugins.Length()));
#ifdef DEBUG
  for (const auto& plugin : plugins) {
    LOGD(("%s::%s plugin: '%s'", __CLASS__, __FUNCTION__,
          plugin->GetDisplayName().get()));
  }
  for (const auto& plugin : mAsyncShutdownPlugins) {
    LOGD(("%s::%s async plugin: '%s'", __CLASS__, __FUNCTION__,
          plugin->GetDisplayName().get()));
  }
#endif
  // Note: CloseActive may be async; it could actually finish
  // shutting down when all the plugins have unloaded.
  for (const auto& plugin : plugins) {
#ifdef MOZ_CRASHREPORTER
    SetAsyncShutdownPluginState(plugin, 'S',
        NS_LITERAL_CSTRING("CloseActive"));
#endif
    plugin->CloseActive(true);
  }

#ifdef MOZ_CRASHREPORTER
      SetAsyncShutdownPluginState(nullptr, '3',
        NS_LITERAL_CSTRING("Dispatching sync-shutdown-complete"));
#endif
  nsCOMPtr<nsIRunnable> task(NewRunnableMethod(
    this, &GeckoMediaPluginServiceParent::NotifySyncShutdownComplete));
  NS_DispatchToMainThread(task);
}

void
GeckoMediaPluginServiceParent::CrashPlugins()
{
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->Crash();
  }
}

RefPtr<GenericPromise::AllPromiseType>
GeckoMediaPluginServiceParent::LoadFromEnvironment()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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
    : mTopic(aTopic)
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
#ifndef MOZ_WIDGET_GONK // Bug 1214967: disabled on B2G due to inscrutable test failures.
  // For e10s, we must fire a notification so that all ContentParents notify
  // their children to update the codecs that the GMPDecoderModule can use.
  NS_DispatchToMainThread(new NotifyObserversTask("gmp-changed"), NS_DISPATCH_NORMAL);
  // For non-e10s, and for decoding in the chrome process, must update GMP
  // PDM's codecs list directly.
  NS_DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
    GMPDecoderModule::UpdateUsableCodecs();
  }));
#endif
  return NS_OK;
}

RefPtr<GenericPromise>
GeckoMediaPluginServiceParent::AsyncAddPluginDirectory(const nsAString& aDirectory)
{
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  if (!thread) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsString dir(aDirectory);
  return InvokeAsync(thread, this, __func__, &GeckoMediaPluginServiceParent::AddOnGMPThread, dir)
    ->Then(AbstractThread::MainThread(), __func__,
      [dir]() -> void {
        LOGD(("GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s succeeded",
              NS_ConvertUTF16toUTF8(dir).get()));
        MOZ_ASSERT(NS_IsMainThread());
        // For e10s, we must fire a notification so that all ContentParents notify
        // their children to update the codecs that the GMPDecoderModule can use.
        nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
        MOZ_ASSERT(obsService);
        if (obsService) {
          obsService->NotifyObservers(nullptr, "gmp-changed", nullptr);
        }
        // For non-e10s, and for decoding in the chrome process, must update GMP
        // PDM's codecs list directly.
        GMPDecoderModule::UpdateUsableCodecs();
      },
      [dir]() -> void {
        LOGD(("GeckoMediaPluginServiceParent::AsyncAddPluginDirectory %s failed",
              NS_ConvertUTF16toUTF8(dir).get()));
      })
    ->CompletionPromise();
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::AddPluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<GenericPromise> p = AsyncAddPluginDirectory(aDirectory);
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
GeckoMediaPluginServiceParent::GetPluginVersionForAPI(const nsACString& aAPI,
                                                      nsTArray<nsCString>* aTags,
                                                      bool* aHasPlugin,
                                                      nsACString& aOutVersion)
{
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aHasPlugin);
  NS_ENSURE_ARG(aOutVersion.IsEmpty());

  nsresult rv = EnsurePluginsOnDiskScanned();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load GMPs from disk.");
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    nsCString api(aAPI);
    size_t index = 0;

    // We must parse the version number into a float for comparison. Yuck.
    double maxParsedVersion = -1.;

    *aHasPlugin = false;
    while (RefPtr<GMPParent> gmp = FindPluginForAPIFrom(index, api, *aTags, &index)) {
      *aHasPlugin = true;
      double parsedVersion = atof(gmp->GetVersion().get());
      if (maxParsedVersion < 0 || parsedVersion > maxParsedVersion) {
        maxParsedVersion = parsedVersion;
        aOutVersion = gmp->GetVersion();
      }
      index++;
    }
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
    nsresult rv = GMPDispatch(new mozilla::Runnable(), NS_DISPATCH_SYNC);
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
    bool supportsAllTags = true;
    for (size_t t = 0; t < aTags.Length(); t++) {
      const nsCString& tag = aTags.ElementAt(t);
      if (!gmp->SupportsAPI(aAPI, tag)) {
        supportsAllTags = false;
        break;
      }
    }
    if (!supportsAllTags) {
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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread,
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
CreateGMPParent()
{
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
  if (!SandboxInfo::Get().CanSandboxMedia()) {
    if (!sAllowInsecureGMP) {
      NS_WARNING("Denying media plugin load due to lack of sandboxing.");
      return nullptr;
    }
    NS_WARNING("Loading media plugin despite lack of sandboxing.");
  }
#endif
  return new GMPParent();
}

already_AddRefed<GMPParent>
GeckoMediaPluginServiceParent::ClonePlugin(const GMPParent* aOriginal)
{
  MOZ_ASSERT(aOriginal);

  RefPtr<GMPParent> gmp = CreateGMPParent();
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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GMPParent> gmp = CreateGMPParent();
  if (!gmp) {
    NS_WARNING("Can't Create GMPParent");
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<GeckoMediaPluginServiceParent> self(this);
  return gmp->Init(this, directory)->Then(thread, __func__,
    [gmp, self, dir]() -> void {
      LOGD(("%s::%s: %s Succeeded", __CLASS__, __FUNCTION__, dir.get()));
      {
        MutexAutoLock lock(self->mMutex);
        self->mPlugins.AppendElement(gmp);
      }
      NS_DispatchToMainThread(new NotifyObserversTask("gmp-path-added"), NS_DISPATCH_NORMAL);
    },
    [dir]() -> void {
      LOGD(("%s::%s: %s Failed", __CLASS__, __FUNCTION__, dir.get()));
    })
    ->CompletionPromise();
}

void
GeckoMediaPluginServiceParent::RemoveOnGMPThread(const nsAString& aDirectory,
                                                 const bool aDeleteFromDisk,
                                                 const bool aCanDefer)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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
  for (size_t i = mPlugins.Length() - 1; i < mPlugins.Length(); i--) {
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
      gmp->AbortAsyncShutdown();
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
      NS_DispatchToMainThread(new NotifyObserversTask("gmp-directory-deleted",
                                                      nsString(aDirectory)),
                              NS_DISPATCH_NORMAL);
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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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
                                         bool aInPrivateBrowsing,
                                         nsACString& aOutId)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: (%s, %s), %s", __CLASS__, __FUNCTION__,
       NS_ConvertUTF16toUTF8(aOrigin).get(),
       NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
       (aInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing")));

  nsresult rv;

  if (aGMPName.EqualsLiteral("gmp-widevinecdm") ||
      aOrigin.EqualsLiteral("null") ||
      aOrigin.IsEmpty() ||
      aTopLevelOrigin.EqualsLiteral("null") ||
      aTopLevelOrigin.IsEmpty()) {
    // This is for the Google Widevine CDM, which doesn't have persistent
    // storage and which can't handle being used by more than one origin at
    // once in the same plugin instance, or at least one of the
    // (origin, topLevelOrigin) is null or empty; probably a local file.
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

  if (aInPrivateBrowsing) {
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
                                         bool aInPrivateBrowsing,
                                         UniquePtr<GetNodeIdCallback>&& aCallback)
{
  nsCString nodeId;
  nsresult rv = GetNodeId(aOrigin, aTopLevelOrigin, aGMPName, aInPrivateBrowsing, nodeId);
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
MatchOrigin(nsIFile* aPath, const nsACString& aSite)
{
  // http://en.wikipedia.org/wiki/Domain_Name_System#Domain_name_syntax
  static const uint32_t MaxDomainLength = 253;

  nsresult rv;
  nsCString str;
  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("origin"), str, MaxDomainLength);
  if (NS_SUCCEEDED(rv) && ExtractHostName(str, str) && str.Equals(aSite)) {
    return true;
  }
  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("topLevelOrigin"), str, MaxDomainLength);
  if (NS_SUCCEEDED(rv) && ExtractHostName(str, str) && str.Equals(aSite)) {
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
    // Abort async shutdown because we're going to wipe the plugin's storage,
    // so we don't want it writing more data in its async shutdown path.
    pluginsToKill[i]->AbortAsyncShutdown();
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
GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread(const nsACString& aSite)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: origin=%s", __CLASS__, __FUNCTION__, aSite.Data()));

  struct OriginFilter : public DirectoryFilter {
    explicit OriginFilter(const nsACString& aSite) : mSite(aSite) {}
    bool operator()(nsIFile* aPath) override {
      return MatchOrigin(aPath, mSite);
    }
  private:
    const nsACString& mSite;
  } filter(aSite);

  ClearNodeIdAndPlugin(filter);
}

void
GeckoMediaPluginServiceParent::ClearRecentHistoryOnGMPThread(PRTime aSince)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: since=%lld", __CLASS__, __FUNCTION__, (int64_t)aSince));

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

  NS_DispatchToMainThread(new NotifyObserversTask("gmp-clear-storage-complete"), NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
GeckoMediaPluginServiceParent::ForgetThisSite(const nsAString& aSite)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(NewRunnableMethod<nsCString>(
      this, &GeckoMediaPluginServiceParent::ForgetThisSiteOnGMPThread,
      NS_ConvertUTF16toUTF8(aSite)));
}

static bool IsNodeIdValid(GMPParent* aParent) {
  return !aParent->GetNodeId().IsEmpty();
}

void
GeckoMediaPluginServiceParent::ClearStorage()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
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

  NS_DispatchToMainThread(new NotifyObserversTask("gmp-clear-storage-complete"), NS_DISPATCH_NORMAL);
}

GMPServiceParent::~GMPServiceParent()
{
  RefPtr<DeleteTask<Transport>> task = new DeleteTask<Transport>(GetTransport());
  XRE_GetIOMessageLoop()->PostTask(task.forget());
}

bool
GMPServiceParent::RecvLoadGMP(const nsCString& aNodeId,
                              const nsCString& aAPI,
                              nsTArray<nsCString>&& aTags,
                              nsTArray<ProcessId>&& aAlreadyBridgedTo,
                              ProcessId* aId,
                              nsCString* aDisplayName,
                              uint32_t* aPluginId,
                              nsresult* aRv)
{
  *aRv = NS_OK;
  if (mService->IsShuttingDown()) {
    *aRv = NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    return true;
  }

  RefPtr<GMPParent> gmp = mService->SelectPluginForAPI(aNodeId, aAPI, aTags);

  nsCString api = aTags[0];
  LOGD(("%s: %p returning %p for api %s", __FUNCTION__, (void *)this, (void *)gmp, api.get()));

  if (!gmp || !gmp->EnsureProcessLoaded(aId)) {
    return false;
  }

  *aDisplayName = gmp->GetDisplayName();
  *aPluginId = gmp->GetPluginId();

  return aAlreadyBridgedTo.Contains(*aId) || gmp->Bridge(this);
}

bool
GMPServiceParent::RecvGetGMPNodeId(const nsString& aOrigin,
                                   const nsString& aTopLevelOrigin,
                                   const nsString& aGMPName,
                                   const bool& aInPrivateBrowsing,
                                   nsCString* aID)
{
  nsresult rv = mService->GetNodeId(aOrigin, aTopLevelOrigin, aGMPName,
                                    aInPrivateBrowsing, *aID);
  return NS_SUCCEEDED(rv);
}

/* static */
bool
GMPServiceParent::RecvGetGMPPluginVersionForAPI(const nsCString& aAPI,
                                                nsTArray<nsCString>&& aTags,
                                                bool* aHasPlugin,
                                                nsCString* aVersion)
{
  RefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  return service &&
         NS_SUCCEEDED(service->GetPluginVersionForAPI(aAPI, &aTags, aHasPlugin,
                                                      *aVersion));
}

class DeleteGMPServiceParent : public mozilla::Runnable
{
public:
  explicit DeleteGMPServiceParent(GMPServiceParent* aToDelete)
    : mToDelete(aToDelete)
  {
  }

  NS_IMETHODIMP Run()
  {
    return NS_OK;
  }

private:
  nsAutoPtr<GMPServiceParent> mToDelete;
};

void
GMPServiceParent::ActorDestroy(ActorDestroyReason aWhy)
{
  NS_DispatchToCurrentThread(new DeleteGMPServiceParent(this));
}

class OpenPGMPServiceParent : public mozilla::Runnable
{
public:
  OpenPGMPServiceParent(GMPServiceParent* aGMPServiceParent,
                        mozilla::ipc::Transport* aTransport,
                        base::ProcessId aOtherPid,
                        bool* aResult)
    : mGMPServiceParent(aGMPServiceParent),
      mTransport(aTransport),
      mOtherPid(aOtherPid),
      mResult(aResult)
  {
  }

  NS_IMETHOD Run()
  {
    *mResult = mGMPServiceParent->Open(mTransport, mOtherPid,
                                       XRE_GetIOMessageLoop(), ipc::ParentSide);
    return NS_OK;
  }

private:
  GMPServiceParent* mGMPServiceParent;
  mozilla::ipc::Transport* mTransport;
  base::ProcessId mOtherPid;
  bool* mResult;
};

/* static */
PGMPServiceParent*
GMPServiceParent::Create(Transport* aTransport, ProcessId aOtherPid)
{
  RefPtr<GeckoMediaPluginServiceParent> gmp =
    GeckoMediaPluginServiceParent::GetSingleton();

  nsAutoPtr<GMPServiceParent> serviceParent(new GMPServiceParent(gmp));

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = gmp->GetThread(getter_AddRefs(gmpThread));
  NS_ENSURE_SUCCESS(rv, nullptr);

  bool ok;
  rv = gmpThread->Dispatch(new OpenPGMPServiceParent(serviceParent,
                                                     aTransport,
                                                     aOtherPid, &ok),
                           NS_DISPATCH_SYNC);
  if (NS_FAILED(rv) || !ok) {
    return nullptr;
  }

  return serviceParent.forget();
}

} // namespace gmp
} // namespace mozilla

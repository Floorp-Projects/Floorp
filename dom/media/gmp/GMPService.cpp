/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "prio.h"
#include "prlog.h"
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

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
PRLogModuleInfo*
GetGMPLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("GMP");
  return sLog;
}

#define LOGD(msg) PR_LOG(GetGMPLog(), PR_LOG_DEBUG, msg)
#define LOG(level, msg) PR_LOG(GetGMPLog(), (level), msg)
#else
#define LOGD(msg)
#define LOG(leve1, msg)
#endif

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPService"

namespace gmp {

static const uint32_t NodeIdSaltLength = 32;
static StaticRefPtr<GeckoMediaPluginService> sSingletonService;

class GMPServiceCreateHelper MOZ_FINAL : public nsRunnable
{
  nsRefPtr<GeckoMediaPluginService> mService;

public:
  static already_AddRefed<GeckoMediaPluginService>
  GetOrCreate()
  {
    nsRefPtr<GeckoMediaPluginService> service;

    if (NS_IsMainThread()) {
      service = GetOrCreateOnMainThread();
    } else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      MOZ_ASSERT(mainThread);

      nsRefPtr<GMPServiceCreateHelper> createHelper = new GMPServiceCreateHelper();

      mozilla::SyncRunnable::DispatchToThread(mainThread, createHelper, true);

      service = createHelper->mService.forget();
    }

    return service.forget();
  }

private:
  GMPServiceCreateHelper()
  {
  }

  ~GMPServiceCreateHelper()
  {
    MOZ_ASSERT(!mService);
  }

  static already_AddRefed<GeckoMediaPluginService>
  GetOrCreateOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsRefPtr<GeckoMediaPluginService> service = sSingletonService.get();
    if (!service) {
      service = new GeckoMediaPluginService();
      service->Init();

      sSingletonService = service;
      ClearOnShutdown(&sSingletonService);
    }

    return service.forget();
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mService = GetOrCreateOnMainThread();
    return NS_OK;
  }
};

already_AddRefed<GeckoMediaPluginService>
GeckoMediaPluginService::GetGeckoMediaPluginService()
{
  return GMPServiceCreateHelper::GetOrCreate();
}

NS_IMPL_ISUPPORTS(GeckoMediaPluginService, mozIGeckoMediaPluginService, nsIObserver)

static int32_t sMaxAsyncShutdownWaitMs = 0;
static bool sHaveSetTimeoutPrefCache = false;

GeckoMediaPluginService::GeckoMediaPluginService()
  : mMutex("GeckoMediaPluginService::mMutex")
  , mShuttingDown(false)
  , mShuttingDownOnGMPThread(false)
  , mScannedPluginOnDisk(false)
  , mWaitingForPluginsAsyncShutdown(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sHaveSetTimeoutPrefCache) {
    sHaveSetTimeoutPrefCache = true;
    Preferences::AddIntVarCache(&sMaxAsyncShutdownWaitMs,
                                "media.gmp.async-shutdown-timeout",
                                GMP_DEFAULT_ASYNC_SHUTDONW_TIMEOUT);
  }
}

GeckoMediaPluginService::~GeckoMediaPluginService()
{
  MOZ_ASSERT(mPlugins.IsEmpty());
  MOZ_ASSERT(mAsyncShutdownPlugins.IsEmpty());
}

int32_t
GeckoMediaPluginService::AsyncShutdownTimeoutMs()
{
  MOZ_ASSERT(sHaveSetTimeoutPrefCache);
  return sMaxAsyncShutdownWaitMs;
}

nsresult
GeckoMediaPluginService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, "profile-change-teardown", false)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, "last-pb-context-exited", false)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, "browser:purge-session-history", false)));

#ifdef DEBUG
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, "mediakeys-request", false)));
#endif

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->AddObserver("media.gmp.plugin.crash", this, false);
  }

#ifndef MOZ_WIDGET_GONK
  // Directory service is main thread only, so cache the profile dir here
  // so that we can use it off main thread.
  // We only do this on non-B2G, as this fails in multi-process Gecko.
  // TODO: Make this work in multi-process Gecko.
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mStorageBaseDir));
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
#endif

  // Kick off scanning for plugins
  nsCOMPtr<nsIThread> thread;
  return GetThread(getter_AddRefs(thread));
}

NS_IMETHODIMP
GeckoMediaPluginService::Observe(nsISupports* aSubject,
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
          gmpThread->Dispatch(WrapRunnable(this, &GeckoMediaPluginService::CrashPlugins),
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
    // On shutdown, we set mWaitingForPluginsAsyncShutdown to true, and then
    // call UnloadPlugins on the GMPThread, and process events on the main
    // thread until an event sets mWaitingForPluginsAsyncShutdown=false on
    // the main thread.
    //
    // UnloadPlugins() sends close messages for all plugins' API objects to
    // the GMP interfaces in the child process, and then sends the async
    // shutdown notifications to child GMPs. When a GMP has completed its
    // shutdown, it calls GMPAsyncShutdownHost::ShutdownComplete(), which
    // sends a message back to the parent, which calls
    // GMPService::AsyncShutdownComplete(). If all plugins requiring async
    // shutdown have called AsyncShutdownComplete() we stick an event on the
    // main thread to set mWaitingForPluginsAsyncShutdown=false. We must use
    // an event to do this, as we must ensure the main thread processes an
    // event to run its loop. This will unblock the main thread, and shutdown
    // of other components will proceed.
    //
    // We set a timer in UnloadPlugins(), and abort waiting for async
    // shutdown if the GMPs are taking too long to shutdown.
    //
    // We shutdown in "profile-change-teardown", as the profile dir is
    // still writable then, and it's required for GMPStorage. We block the
    // shutdown process by spinning the main thread event loop until all GMPs
    // have shutdown, or timeout has occurred.
    //
    // GMPStorage needs to work up until the shutdown-complete notification
    // arrives from the GMP process.

    mWaitingForPluginsAsyncShutdown = true;

    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mShuttingDown);
      mShuttingDown = true;
      gmpThread = mGMPThread;
    }

    if (gmpThread) {
      gmpThread->Dispatch(
        NS_NewRunnableMethod(this, &GeckoMediaPluginService::UnloadPlugins),
        NS_DISPATCH_NORMAL);
    } else {
      MOZ_ASSERT(mPlugins.IsEmpty());
    }

    // Wait for plugins to do async shutdown...
    while (mWaitingForPluginsAsyncShutdown) {
      NS_ProcessNextEvent(NS_GetCurrentThread(), true);
    }

  } else if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      // XXX The content process never gets profile-change-teardown, so mShuttingDown
      // will always be false here. GMPService needs to be proxied to the parent.
      // See bug 1057908.
      MOZ_ASSERT(XRE_GetProcessType() != GeckoProcessType_Default || mShuttingDown);
      mGMPThread.swap(gmpThread);
    }

    if (gmpThread) {
      gmpThread->Shutdown();
    }
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
      return GMPDispatch(NS_NewRunnableMethod(
          this, &GeckoMediaPluginService::ClearStorage));
    }

    // Clear nodeIds/records modified after |t|.
    nsresult rv;
    PRTime t = nsDependentString(aSomeData).ToInteger64(&rv, 10);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return GMPDispatch(NS_NewRunnableMethodWithArg<PRTime>(
        this, &GeckoMediaPluginService::ClearRecentHistoryOnGMPThread, t));
  }

  return NS_OK;
}

nsresult
GeckoMediaPluginService::GMPDispatch(nsIRunnable* event, uint32_t flags)
{
  nsCOMPtr<nsIRunnable> r(event);
  nsCOMPtr<nsIThread> thread;
  nsresult rv = GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return rv;
  }
  return thread->Dispatch(r, flags);
}

// always call with getter_AddRefs, because it does
NS_IMETHODIMP
GeckoMediaPluginService::GetThread(nsIThread** aThread)
{
  MOZ_ASSERT(aThread);

  // This can be called from any thread.
  MutexAutoLock lock(mMutex);

  if (!mGMPThread) {
    // Don't allow the thread to be created after shutdown has started.
    if (mShuttingDown) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = NS_NewNamedThread("GMPThread", getter_AddRefs(mGMPThread));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Tell the thread to initialize plugins
    mGMPThread->Dispatch(NS_NewRunnableMethod(this, &GeckoMediaPluginService::LoadFromEnvironment), NS_DISPATCH_NORMAL);
  }

  NS_ADDREF(mGMPThread);
  *aThread = mGMPThread;

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPAudioDecoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            GMPAudioDecoderProxy** aGMPAD)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aGMPAD);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aNodeId,
                                               NS_LITERAL_CSTRING(GMP_API_AUDIO_DECODER),
                                               *aTags);
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }

  GMPAudioDecoderParent* gmpADP;
  nsresult rv = gmp->GetGMPAudioDecoder(&gmpADP);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aGMPAD = gmpADP;

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoDecoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            GMPVideoHost** aOutVideoHost,
                                            GMPVideoDecoderProxy** aGMPVD)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aOutVideoHost);
  NS_ENSURE_ARG(aGMPVD);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aNodeId,
                                               NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                                               *aTags);
#ifdef PR_LOGGING
  nsCString api = (*aTags)[0];
  LOGD(("%s: %p returning %p for api %s", __FUNCTION__, (void *)this, (void *)gmp, api.get()));
#endif
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }


  GMPVideoDecoderParent* gmpVDP;
  nsresult rv = gmp->GetGMPVideoDecoder(&gmpVDP);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aGMPVD = gmpVDP;
  *aOutVideoHost = &gmpVDP->Host();

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoEncoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            GMPVideoHost** aOutVideoHost,
                                            GMPVideoEncoderProxy** aGMPVE)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aOutVideoHost);
  NS_ENSURE_ARG(aGMPVE);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aNodeId,
                                               NS_LITERAL_CSTRING(GMP_API_VIDEO_ENCODER),
                                               *aTags);
#ifdef PR_LOGGING
  nsCString api = (*aTags)[0];
  LOGD(("%s: %p returning %p for api %s", __FUNCTION__, (void *)this, (void *)gmp, api.get()));
#endif
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }

  GMPVideoEncoderParent* gmpVEP;
  nsresult rv = gmp->GetGMPVideoEncoder(&gmpVEP);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aGMPVE = gmpVEP;
  *aOutVideoHost = &gmpVEP->Host();

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPDecryptor(nsTArray<nsCString>* aTags,
                                         const nsACString& aNodeId,
                                         GMPDecryptorProxy** aDecryptor)
{
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
  if (!SandboxInfo::Get().CanSandboxMedia()) {
    NS_WARNING("GeckoMediaPluginService::GetGMPDecryptor: "
               "EME decryption not available without sandboxing support.");
    return NS_ERROR_NOT_AVAILABLE;
  }
#endif

  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aDecryptor);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aNodeId,
                                               NS_LITERAL_CSTRING(GMP_API_DECRYPTOR),
                                               *aTags);
  if (!gmp) {
    return NS_ERROR_FAILURE;
  }

  GMPDecryptorParent* ksp;
  nsresult rv = gmp->GetGMPDecryptor(&ksp);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aDecryptor = static_cast<GMPDecryptorProxy*>(ksp);

  return NS_OK;
}

void
GeckoMediaPluginService::AsyncShutdownNeeded(GMPParent* aParent)
{
  LOGD(("%s::%s %p", __CLASS__, __FUNCTION__, aParent));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MOZ_ASSERT(!mAsyncShutdownPlugins.Contains(aParent));
  mAsyncShutdownPlugins.AppendElement(aParent);
}

void
GeckoMediaPluginService::AsyncShutdownComplete(GMPParent* aParent)
{
  LOGD(("%s::%s %p", __CLASS__, __FUNCTION__, aParent));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  mAsyncShutdownPlugins.RemoveElement(aParent);
  if (mAsyncShutdownPlugins.IsEmpty() && mShuttingDownOnGMPThread) {
    // The main thread may be waiting for async shutdown of plugins,
    // which has completed. Break the main thread out of its waiting loop.
    nsRefPtr<nsIRunnable> task(NS_NewRunnableMethod(
      this, &GeckoMediaPluginService::SetAsyncShutdownComplete));
    NS_DispatchToMainThread(task);
  }
}

void
GeckoMediaPluginService::SetAsyncShutdownComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  mWaitingForPluginsAsyncShutdown = false;
}

void
GeckoMediaPluginService::UnloadPlugins()
{
  LOGD(("%s::%s async_shutdown=%d", __CLASS__, __FUNCTION__,
        mAsyncShutdownPlugins.Length()));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MOZ_ASSERT(!mShuttingDownOnGMPThread);
  mShuttingDownOnGMPThread = true;

  {
    MutexAutoLock lock(mMutex);
    // Note: CloseActive is async; it will actually finish
    // shutting down when all the plugins have unloaded.
    for (size_t i = 0; i < mPlugins.Length(); i++) {
      mPlugins[i]->CloseActive(true);
    }
    mPlugins.Clear();
  }

  if (mAsyncShutdownPlugins.IsEmpty()) {
    nsRefPtr<nsIRunnable> task(NS_NewRunnableMethod(
      this, &GeckoMediaPluginService::SetAsyncShutdownComplete));
    NS_DispatchToMainThread(task);
  }
}

void
GeckoMediaPluginService::CrashPlugins()
{
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->Crash();
  }
}

void
GeckoMediaPluginService::LoadFromEnvironment()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  const char* env = PR_GetEnv("MOZ_GMP_PATH");
  if (!env || !*env) {
    return;
  }

  nsString allpaths;
  if (NS_WARN_IF(NS_FAILED(NS_CopyNativeToUnicode(nsDependentCString(env), allpaths)))) {
    return;
  }

  uint32_t pos = 0;
  while (pos < allpaths.Length()) {
    // Loop over multiple path entries separated by colons (*nix) or
    // semicolons (Windows)
    int32_t next = allpaths.FindChar(XPCOM_ENV_PATH_SEPARATOR[0], pos);
    if (next == -1) {
      AddOnGMPThread(nsDependentSubstring(allpaths, pos));
      break;
    } else {
      AddOnGMPThread(nsDependentSubstring(allpaths, pos, next - pos));
      pos = next + 1;
    }
  }

  mScannedPluginOnDisk = true;
}

NS_IMETHODIMP
GeckoMediaPluginService::PathRunnable::Run()
{
  if (mAdd) {
    mService->AddOnGMPThread(mPath);
  } else {
    mService->RemoveOnGMPThread(mPath);
  }
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::AddPluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(new PathRunnable(this, aDirectory, true));
}

NS_IMETHODIMP
GeckoMediaPluginService::RemovePluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(new PathRunnable(this, aDirectory, false));
}

class DummyRunnable : public nsRunnable {
public:
  NS_IMETHOD Run() { return NS_OK; }
};

NS_IMETHODIMP
GeckoMediaPluginService::GetPluginVersionForAPI(const nsACString& aAPI,
                                                nsTArray<nsCString>* aTags,
                                                nsACString& aOutVersion)
{
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);

  nsresult rv = EnsurePluginsOnDiskScanned();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load GMPs from disk.");
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    nsCString api(aAPI);
    GMPParent* gmp = FindPluginForAPIFrom(0, api, *aTags, nullptr);
    if (!gmp) {
      return NS_ERROR_FAILURE;
    }
    aOutVersion = gmp->GetVersion();
  }

  return NS_OK;
}

nsresult
GeckoMediaPluginService::EnsurePluginsOnDiskScanned()
{
  const char* env = nullptr;
  if (!mScannedPluginOnDisk && (env = PR_GetEnv("MOZ_GMP_PATH")) && *env) {
    // We have a MOZ_GMP_PATH environment variable which may specify the
    // location of plugins to load, and we haven't yet scanned the disk to
    // see if there are plugins there. Get the GMP thread, which will
    // cause an event to be dispatched to which scans for plugins. We
    // dispatch a sync event to the GMP thread here in order to wait until
    // after the GMP thread has scanned any paths in MOZ_GMP_PATH.
    nsresult rv = GMPDispatch(new DummyRunnable(), NS_DISPATCH_SYNC);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(mScannedPluginOnDisk, "Should have scanned MOZ_GMP_PATH by now");
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::HasPluginForAPI(const nsACString& aAPI,
                                         nsTArray<nsCString>* aTags,
                                         bool* aOutHavePlugin)
{
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aOutHavePlugin);

  nsresult rv = EnsurePluginsOnDiskScanned();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to load GMPs from disk.");
    return rv;
  }

  {
    MutexAutoLock lock(mMutex);
    nsCString api(aAPI);
    GMPParent* gmp = FindPluginForAPIFrom(0, api, *aTags, nullptr);
    *aOutHavePlugin = (gmp != nullptr);
  }

  return NS_OK;
}

GMPParent*
GeckoMediaPluginService::FindPluginForAPIFrom(size_t aSearchStartIndex,
                                              const nsCString& aAPI,
                                              const nsTArray<nsCString>& aTags,
                                              size_t* aOutPluginIndex)
{
  mMutex.AssertCurrentThreadOwns();
  for (size_t i = aSearchStartIndex; i < mPlugins.Length(); i++) {
    GMPParent* gmp = mPlugins[i];
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
    return gmp;
  }
  return nullptr;
}

GMPParent*
GeckoMediaPluginService::SelectPluginForAPI(const nsACString& aNodeId,
                                            const nsCString& aAPI,
                                            const nsTArray<nsCString>& aTags)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread,
             "Can't clone GMP plugins on non-GMP threads.");

  GMPParent* gmpToClone = nullptr;
  {
    MutexAutoLock lock(mMutex);
    size_t index = 0;
    GMPParent* gmp = nullptr;
    while ((gmp = FindPluginForAPIFrom(index, aAPI, aTags, &index))) {
      if (aNodeId.IsEmpty()) {
        if (gmp->CanBeSharedCrossNodeIds()) {
          return gmp;
        }
      } else if (gmp->CanBeUsedFrom(aNodeId)) {
        MOZ_ASSERT(!aNodeId.IsEmpty());
        gmp->SetNodeId(aNodeId);
        return gmp;
      }

      // This GMP has the correct type but has the wrong nodeId; hold on to it
      // in case we need to clone it.
      gmpToClone = gmp;
      // Loop around and try the next plugin; it may be usable from aNodeId.
      index++;
    }
  }

  // Plugin exists, but we can't use it due to cross-origin separation. Create a
  // new one.
  if (gmpToClone) {
    GMPParent* clone = ClonePlugin(gmpToClone);
    if (!aNodeId.IsEmpty()) {
      clone->SetNodeId(aNodeId);
    }
    return clone;
  }

  return nullptr;
}

class CreateGMPParentTask : public nsRunnable {
public:
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
    if (!SandboxInfo::Get().CanSandboxMedia()) {
      if (!Preferences::GetBool("media.gmp.insecure.allow")) {
        NS_WARNING("Denying media plugin load due to lack of sandboxing.");
        return NS_ERROR_NOT_AVAILABLE;
      }
      NS_WARNING("Loading media plugin despite lack of sandboxing.");
    }
#endif
    mParent = new GMPParent();
    return NS_OK;
  }
  already_AddRefed<GMPParent> GetParent() {
    return mParent.forget();
  }
private:
  nsRefPtr<GMPParent> mParent;
};

GMPParent*
GeckoMediaPluginService::ClonePlugin(const GMPParent* aOriginal)
{
  MOZ_ASSERT(aOriginal);

  // The GMPParent inherits from IToplevelProtocol, which must be created
  // on the main thread to be threadsafe. See Bug 1035653.
  nsRefPtr<CreateGMPParentTask> task(new CreateGMPParentTask());
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    MOZ_ASSERT(mainThread);
    mozilla::SyncRunnable::DispatchToThread(mainThread, task);
  }

  nsRefPtr<GMPParent> gmp = task->GetParent();
  nsresult rv = gmp->CloneFrom(aOriginal);

  if (NS_FAILED(rv)) {
    NS_WARNING("Can't Create GMPParent");
    return nullptr;
  }

  MutexAutoLock lock(mMutex);
  mPlugins.AppendElement(gmp);

  return gmp.get();
}

void
GeckoMediaPluginService::AddOnGMPThread(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: %s", __CLASS__, __FUNCTION__, NS_LossyConvertUTF16toASCII(aDirectory).get()));

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // The GMPParent inherits from IToplevelProtocol, which must be created
  // on the main thread to be threadsafe. See Bug 1035653.
  nsRefPtr<CreateGMPParentTask> task(new CreateGMPParentTask());
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  MOZ_ASSERT(mainThread);
  mozilla::SyncRunnable::DispatchToThread(mainThread, task);
  nsRefPtr<GMPParent> gmp = task->GetParent();
  rv = gmp ? gmp->Init(this, directory) : NS_ERROR_NOT_AVAILABLE;
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't Create GMPParent");
    return;
  }

  MutexAutoLock lock(mMutex);
  mPlugins.AppendElement(gmp);
}

void
GeckoMediaPluginService::RemoveOnGMPThread(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: %s", __CLASS__, __FUNCTION__, NS_LossyConvertUTF16toASCII(aDirectory).get()));

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mPlugins.Length(); ++i) {
    nsCOMPtr<nsIFile> pluginpath = mPlugins[i]->GetDirectory();
    bool equals;
    if (NS_SUCCEEDED(directory->Equals(pluginpath, &equals)) && equals) {
      mPlugins[i]->CloseActive(true);
      mPlugins.RemoveElementAt(i);
      return;
    }
  }
  NS_WARNING("Removing GMP which was never added.");
  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  cs->LogStringMessage(MOZ_UTF16("Removing GMP which was never added."));
}

// May remove when Bug 1043671 is fixed
static void Dummy(nsRefPtr<GMPParent>& aOnDeathsDoor)
{
  // exists solely to do nothing and let the Runnable kill the GMPParent
  // when done.
}

void
GeckoMediaPluginService::ReAddOnGMPThread(nsRefPtr<GMPParent>& aOld)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, (void*) aOld));

  nsRefPtr<GMPParent> gmp;
  if (!mShuttingDownOnGMPThread) {
    // Don't re-add plugin if we're shutting down. Let the old plugin die.
    gmp = ClonePlugin(aOld);
  }
  // Note: both are now in the list
  // Until we give up the GMPThread, we're safe even if we unlock temporarily
  // since off-main-thread users just test for existance; they don't modify the list.
  MutexAutoLock lock(mMutex);
  mPlugins.RemoveElement(aOld);

  // Schedule aOld to be destroyed.  We can't destroy it from here since we
  // may be inside ActorDestroyed() for it.
  NS_DispatchToCurrentThread(WrapRunnableNM(&Dummy, aOld));
}

NS_IMETHODIMP
GeckoMediaPluginService::GetStorageDir(nsIFile** aOutFile)
{
#ifndef MOZ_WIDGET_GONK
  if (NS_WARN_IF(!mStorageBaseDir)) {
    return NS_ERROR_FAILURE;
  }
  return mStorageBaseDir->Clone(aOutFile);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

NS_IMETHODIMP
GeckoMediaPluginService::IsPersistentStorageAllowed(const nsACString& aNodeId,
                                                    bool* aOutAllowed)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aOutAllowed);
  *aOutAllowed = mPersistentStorageAllowed.Get(aNodeId);
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetNodeId(const nsAString& aOrigin,
                                   const nsAString& aTopLevelOrigin,
                                   bool aInPrivateBrowsing,
                                   nsACString& aOutId)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: (%s, %s), %s", __CLASS__, __FUNCTION__,
       NS_ConvertUTF16toUTF8(aOrigin).get(),
       NS_ConvertUTF16toUTF8(aTopLevelOrigin).get(),
       (aInPrivateBrowsing ? "PrivateBrowsing" : "NonPrivateBrowsing")));

#ifdef MOZ_WIDGET_GONK
  NS_WARNING("GeckoMediaPluginService::GetNodeId Not implemented on B2G");
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  nsresult rv;

  if (aOrigin.EqualsLiteral("null") ||
      aOrigin.IsEmpty() ||
      aTopLevelOrigin.EqualsLiteral("null") ||
      aTopLevelOrigin.IsEmpty()) {
    // At least one of the (origin, topLevelOrigin) is null or empty;
    // probably a local file. Generate a random node id, and don't store
    // it so that the GMP's storage is temporary and not shared.
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
    // For PB mode, we store the node id, indexed by the origin pair,
    // so that if the same origin pair is opened in this session, it gets
    // the same node id.
    nsCString* salt = nullptr;
    if (!(salt = mTempNodeIds.Get(hash))) {
      // No salt stored, generate and temporarily store some for this id.
      nsAutoCString newSalt;
      rv = GenerateRandomPathName(newSalt, NodeIdSaltLength);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      salt = new nsCString(newSalt);
      mTempNodeIds.Put(hash, salt);
      mPersistentStorageAllowed.Put(*salt, false);
    }
    aOutId = *salt;
    return NS_OK;
  }

  // Otherwise, try to see if we've previously generated and stored salt
  // for this origin pair.
  nsCOMPtr<nsIFile> path; // $profileDir/gmp/
  rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = path->AppendNative(NS_LITERAL_CSTRING("id"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // $profileDir/gmp/id/
  rv = path->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (rv != NS_ERROR_FILE_ALREADY_EXISTS && NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString hashStr;
  hashStr.AppendInt((int64_t)hash);

  // $profileDir/gmp/id/$hash
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

    // $profileDir/gmp/id/$hash/salt
    rv = WriteToFile(path, NS_LITERAL_CSTRING("salt"), salt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/id/$hash/origin
    rv = WriteToFile(path,
                     NS_LITERAL_CSTRING("origin"),
                     NS_ConvertUTF16toUTF8(aOrigin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // $profileDir/gmp/id/$hash/topLevelOrigin
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

bool
MatchOrigin(nsIFile* aPath, const nsACString& aOrigin)
{
  // http://en.wikipedia.org/wiki/Domain_Name_System#Domain_name_syntax
  static const uint32_t MaxDomainLength = 253;

  nsresult rv;
  nsCString str;
  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("origin"), str, MaxDomainLength);
  if (NS_SUCCEEDED(rv) && aOrigin.Equals(str)) {
    return true;
  }
  rv = ReadFromFile(aPath, NS_LITERAL_CSTRING("topLevelOrigin"), str, MaxDomainLength);
  if (NS_SUCCEEDED(rv) && aOrigin.Equals(str)) {
    return true;
  }
  return false;
}

template<typename T> static void
KillPlugins(const nsTArray<nsRefPtr<GMPParent>>& aPlugins,
            Mutex& aMutex, T&& aFilter)
{
  // Shutdown the plugins when |aFilter| evaluates to true.
  // After we clear storage data, node IDs will become invalid and shouldn't be
  // used anymore. We need to kill plugins with such nodeIDs.
  // Note: we can't shut them down while holding the lock,
  // as the lock is not re-entrant and shutdown requires taking the lock.
  // The plugin list is only edited on the GMP thread, so this should be OK.
  nsTArray<nsRefPtr<GMPParent>> pluginsToKill;
  {
    MutexAutoLock lock(aMutex);
    for (size_t i = 0; i < aPlugins.Length(); i++) {
      nsRefPtr<GMPParent> parent(aPlugins[i]);
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
GeckoMediaPluginService::ClearNodeIdAndPlugin(DirectoryFilter& aFilter)
{
  nsresult rv;
  nsCOMPtr<nsIFile> path;

  // $profileDir/gmp/
  rv = GetStorageDir(getter_AddRefs(path));
  if (NS_FAILED(rv)) {
    return;
  }

  // $profileDir/gmp/id/
  rv = path->AppendNative(NS_LITERAL_CSTRING("id"));
  if (NS_FAILED(rv)) {
    return;
  }

  // Iterate all sub-folders of $profileDir/gmp/id/
  nsCOMPtr<nsISimpleEnumerator> iter;
  rv = path->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    return;
  }

  bool hasMore = false;
  nsTArray<nsCString> nodeIDsToClear;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) {
      continue;
    }

    // $profileDir/gmp/id/$hash
    nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) {
      continue;
    }

    // Skip non-directory files.
    bool isDirectory = false;
    rv = dirEntry->IsDirectory(&isDirectory);
    if (NS_FAILED(rv) || !isDirectory) {
      continue;
    }

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

  // Kill plugins that have node IDs to be cleared.
  KillPlugins(mPlugins, mMutex, NodeFilter(nodeIDsToClear));

  // Clear all matching $profileDir/gmp/storage/$nodeId/
  rv = GetStorageDir(getter_AddRefs(path));
  if (NS_FAILED(rv)) {
    return;
  }

  rv = path->AppendNative(NS_LITERAL_CSTRING("storage"));
  if (NS_FAILED(rv)) {
    return;
  }

  for (size_t i = 0; i < nodeIDsToClear.Length(); i++) {
    nsCOMPtr<nsIFile> dirEntry;
    rv = path->Clone(getter_AddRefs(dirEntry));
    if (NS_FAILED(rv)) {
      continue;
    }

    rv = dirEntry->AppendNative(nodeIDsToClear[i]);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (NS_FAILED(DeleteDir(dirEntry))) {
      NS_WARNING("Failed to delete GMP storage directory for the node");
    }
  }
}

void
GeckoMediaPluginService::ForgetThisSiteOnGMPThread(const nsACString& aOrigin)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: origin=%s", __CLASS__, __FUNCTION__, aOrigin.Data()));

  struct OriginFilter : public DirectoryFilter {
    explicit OriginFilter(const nsACString& aOrigin) : mOrigin(aOrigin) {}
    virtual bool operator()(nsIFile* aPath) {
      return MatchOrigin(aPath, mOrigin);
    }
  private:
    const nsACString& mOrigin;
  } filter(aOrigin);

  ClearNodeIdAndPlugin(filter);
}

class StorageClearedTask : public nsRunnable {
public:
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
    MOZ_ASSERT(obsService);
    if (obsService) {
      obsService->NotifyObservers(nullptr, "gmp-clear-storage-complete", nullptr);
    }
    return NS_OK;
  }
};

void
GeckoMediaPluginService::ClearRecentHistoryOnGMPThread(PRTime aSince)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s: since=%lld", __CLASS__, __FUNCTION__, (int64_t)aSince));

  nsCOMPtr<nsIFile> storagePath;
  nsCOMPtr<nsIFile> temp;
  if (NS_SUCCEEDED(GetStorageDir(getter_AddRefs(temp))) &&
      NS_SUCCEEDED(temp->AppendNative(NS_LITERAL_CSTRING("storage")))) {
    storagePath = temp.forget();
  }

  struct MTimeFilter : public DirectoryFilter {
    explicit MTimeFilter(PRTime aSince, already_AddRefed<nsIFile> aPath)
      : mSince(aSince), mStoragePath(aPath) {}

    // Return true if any files under aPath is modified after |mSince|.
    bool IsModifiedAfter(nsIFile* aPath) {
      PRTime lastModified;
      nsresult rv = aPath->GetLastModifiedTime(&lastModified);
      if (NS_SUCCEEDED(rv) && lastModified >= mSince) {
        return true;
      }
      // Check sub-directories recursively
      nsCOMPtr<nsISimpleEnumerator> iter;
      rv = aPath->GetDirectoryEntries(getter_AddRefs(iter));
      if (NS_FAILED(rv)) {
        return false;
      }

      bool hasMore = false;
      while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        rv = iter->GetNext(getter_AddRefs(supports));
        if (NS_FAILED(rv)) {
          continue;
        }

        nsCOMPtr<nsIFile> path(do_QueryInterface(supports, &rv));
        if (NS_FAILED(rv)) {
          continue;
        }

        if (IsModifiedAfter(path)) {
          return true;
        }
      }
      return false;
    }

    // |aPath| is $profileDir/gmp/id/$hash
    virtual bool operator()(nsIFile* aPath) {
      if (IsModifiedAfter(aPath)) {
        return true;
      }

      nsAutoCString salt;
      nsresult rv = ReadSalt(aPath, salt);
      if (NS_FAILED(rv)) {
        return false;
      }

      // $profileDir/gmp/storage/
      if (!mStoragePath) {
        return false;
      }
      // $profileDir/gmp/storage/$nodeId/
      nsCOMPtr<nsIFile> path;
      rv = mStoragePath->Clone(getter_AddRefs(path));
      if (NS_FAILED(rv)) {
        return false;
      }

      rv = path->AppendNative(salt);
      return NS_SUCCEEDED(rv) && IsModifiedAfter(path);
    }
  private:
    const PRTime mSince;
    const nsCOMPtr<nsIFile> mStoragePath;
  } filter(aSince, storagePath.forget());

  ClearNodeIdAndPlugin(filter);

  NS_DispatchToMainThread(new StorageClearedTask(), NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
GeckoMediaPluginService::ForgetThisSite(const nsAString& aOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());
  return GMPDispatch(NS_NewRunnableMethodWithArg<nsCString>(
      this, &GeckoMediaPluginService::ForgetThisSiteOnGMPThread,
      NS_ConvertUTF16toUTF8(aOrigin)));
}

static bool IsNodeIdValid(GMPParent* aParent) {
  return !aParent->GetNodeId().IsEmpty();
}

void
GeckoMediaPluginService::ClearStorage()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));

#ifdef MOZ_WIDGET_GONK
  NS_WARNING("GeckoMediaPluginService::ClearStorage not implemented on B2G");
  return;
#endif

  // Kill plugins with valid nodeIDs.
  KillPlugins(mPlugins, mMutex, &IsNodeIdValid);

  nsCOMPtr<nsIFile> path; // $profileDir/gmp/
  nsresult rv = GetStorageDir(getter_AddRefs(path));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_FAILED(DeleteDir(path))) {
    NS_WARNING("Failed to delete GMP storage directory");
  }
  NS_DispatchToMainThread(new StorageClearedTask(), NS_DISPATCH_NORMAL);
}

} // namespace gmp
} // namespace mozilla

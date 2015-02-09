/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceParent.h"
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

static StaticRefPtr<GeckoMediaPluginService> sSingletonService;

class GMPServiceCreateHelper final : public nsRunnable
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

    if (!sSingletonService) {
      nsRefPtr<GeckoMediaPluginServiceParent> service =
        new GeckoMediaPluginServiceParent();
      service->Init();

      sSingletonService = service;
      ClearOnShutdown(&sSingletonService);
    }

    nsRefPtr<GeckoMediaPluginService> service = sSingletonService.get();
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

GeckoMediaPluginService::GeckoMediaPluginService()
  : mMutex("GeckoMediaPluginService::mMutex")
  , mGMPThreadShutdown(false)
  , mShuttingDownOnGMPThread(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

GeckoMediaPluginService::~GeckoMediaPluginService()
{
}

void
GeckoMediaPluginService::RemoveObsoletePluginCrashCallbacks()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = mPluginCrashCallbacks.Length(); i != 0; --i) {
    nsRefPtr<PluginCrashCallback>& callback = mPluginCrashCallbacks[i - 1];
    if (!callback->IsStillValid()) {
      LOGD(("%s::%s - Removing obsolete callback for pluginId %s",
            __CLASS__, __FUNCTION__,
            PromiseFlatCString(callback->PluginId()).get()));
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    }
  }
}

void
GeckoMediaPluginService::AddPluginCrashCallback(
  nsRefPtr<PluginCrashCallback> aPluginCrashCallback)
{
  RemoveObsoletePluginCrashCallbacks();
  mPluginCrashCallbacks.AppendElement(aPluginCrashCallback);
}

void
GeckoMediaPluginService::RemovePluginCrashCallbacks(const nsACString& aPluginId)
{
  RemoveObsoletePluginCrashCallbacks();
  for (size_t i = mPluginCrashCallbacks.Length(); i != 0; --i) {
    nsRefPtr<PluginCrashCallback>& callback = mPluginCrashCallbacks[i - 1];
    if (callback->PluginId() == aPluginId) {
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    }
  }
}

void
GeckoMediaPluginService::RunPluginCrashCallbacks(const nsACString& aPluginId,
                                                 const nsACString& aPluginName,
                                                 const nsAString& aPluginDumpId)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOGD(("%s::%s(%s)", __CLASS__, __FUNCTION__, aPluginId.Data()));
  for (size_t i = mPluginCrashCallbacks.Length(); i != 0; --i) {
    nsRefPtr<PluginCrashCallback>& callback = mPluginCrashCallbacks[i - 1];
    const nsACString& callbackPluginId = callback->PluginId();
    if (!callback->IsStillValid()) {
      LOGD(("%s::%s(%s) - Removing obsolete callback for pluginId %s",
            __CLASS__, __FUNCTION__, aPluginId.Data(),
            PromiseFlatCString(callback->PluginId()).get()));
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    } else if (callbackPluginId == aPluginId) {
      LOGD(("%s::%s(%s) - Running #%u",
          __CLASS__, __FUNCTION__, aPluginId.Data(), i - 1));
      callback->Run(aPluginName, aPluginDumpId);
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    }
  }
}

nsresult
GeckoMediaPluginService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false)));

  // Kick off scanning for plugins
  nsCOMPtr<nsIThread> thread;
  return GetThread(getter_AddRefs(thread));
}

void
GeckoMediaPluginService::ShutdownGMPThread()
{
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));
  nsCOMPtr<nsIThread> gmpThread;
  {
    MutexAutoLock lock(mMutex);
    mGMPThreadShutdown = true;
    mGMPThread.swap(gmpThread);
  }

  if (gmpThread) {
    gmpThread->Shutdown();
  }
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
    if (mGMPThreadShutdown) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = NS_NewNamedThread("GMPThread", getter_AddRefs(mGMPThread));
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Tell the thread to initialize plugins
    InitializePlugins();
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

NS_IMETHODIMP
GeckoMediaPluginService::HasPluginForAPI(const nsACString& aAPI,
                                         nsTArray<nsCString>* aTags,
                                         bool* aOutHavePlugin)
{
  nsCString unused;
  return GetPluginVersionForAPI(aAPI, aTags, aOutHavePlugin, unused);
}

} // namespace gmp
} // namespace mozilla

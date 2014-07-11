/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "nsIObserverService.h"
#include "GeckoChildProcessHost.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/Services.h"
#include "nsNativeCharsetUtils.h"
#include "nsIConsoleService.h"

namespace mozilla {
namespace gmp {

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

GeckoMediaPluginService::GeckoMediaPluginService()
  : mMutex("GeckoMediaPluginService::mMutex")
  , mShuttingDown(false)
  , mShuttingDownOnGMPThread(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

GeckoMediaPluginService::~GeckoMediaPluginService()
{
  MOZ_ASSERT(mPlugins.IsEmpty());
}

void
GeckoMediaPluginService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false)));
}

NS_IMETHODIMP
GeckoMediaPluginService::Observe(nsISupports* aSubject,
                                 const char* aTopic,
                                 const char16_t* aSomeData)
{
  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mShuttingDown);
      mShuttingDown = true;
      gmpThread = mGMPThread;
    }

    if (gmpThread) {
      gmpThread->Dispatch(NS_NewRunnableMethod(this, &GeckoMediaPluginService::UnloadPlugins),
                           NS_DISPATCH_SYNC);
    } else {
      MOZ_ASSERT(mPlugins.IsEmpty());
    }
  } else if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    nsCOMPtr<nsIThread> gmpThread;
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(mShuttingDown);
      mGMPThread.swap(gmpThread);
    }

    if (gmpThread) {
      gmpThread->Shutdown();
    }
  }
  return NS_OK;
}

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
GeckoMediaPluginService::GetGMPVideoDecoder(nsTArray<nsCString>* aTags,
                                            const nsAString& aOrigin,
                                            GMPVideoHost** aOutVideoHost,
                                            GMPVideoDecoder** aGMPVD)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aOutVideoHost);
  NS_ENSURE_ARG(aGMPVD);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aOrigin,
                                               NS_LITERAL_CSTRING("decode-video"),
                                               *aTags);
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
                                            const nsAString& aOrigin,
                                            GMPVideoHost** aOutVideoHost,
                                            GMPVideoEncoder** aGMPVE)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aOutVideoHost);
  NS_ENSURE_ARG(aGMPVE);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(aOrigin,
                                               NS_LITERAL_CSTRING("encode-video"),
                                               *aTags);
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

void
GeckoMediaPluginService::UnloadPlugins()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  MOZ_ASSERT(!mShuttingDownOnGMPThread);
  mShuttingDownOnGMPThread = true;

  for (uint32_t i = 0; i < mPlugins.Length(); i++) {
    mPlugins[i]->UnloadProcess();
  }
  mPlugins.Clear();
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
  nsCOMPtr<nsIThread> thread;
  nsresult rv = GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIRunnable> r = new PathRunnable(this, aDirectory, true);
  thread->Dispatch(r, NS_DISPATCH_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::RemovePluginDirectory(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIThread> thread;
  nsresult rv = GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIRunnable> r = new PathRunnable(this, aDirectory, false);
  thread->Dispatch(r, NS_DISPATCH_NORMAL);
  return NS_OK;
}

GMPParent*
GeckoMediaPluginService::SelectPluginForAPI(const nsAString& aOrigin,
                                            const nsCString& aAPI,
                                            const nsTArray<nsCString>& aTags)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  for (uint32_t i = 0; i < mPlugins.Length(); i++) {
    GMPParent* gmp = mPlugins[i];
    bool supportsAllTags = true;
    for (uint32_t t = 0; t < aTags.Length(); t++) {
      const nsCString& tag = aTags[t];
      if (!gmp->SupportsAPI(aAPI, tag)) {
        supportsAllTags = false;
        break;
      }
    }
    if (!supportsAllTags) {
      continue;
    }
    if (aOrigin.IsEmpty()) {
      if (gmp->CanBeSharedCrossOrigin()) {
        return gmp;
      }
    } else if (gmp->CanBeUsedFrom(aOrigin)) {
      if (!aOrigin.IsEmpty()) {
        gmp->SetOrigin(aOrigin);
      }
      return gmp;
    }
  }
  return nullptr;
}


void
GeckoMediaPluginService::AddOnGMPThread(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsRefPtr<GMPParent> gmp = new GMPParent();
  rv = gmp->Init(directory);
  if (NS_FAILED(rv)) {
    return;
  }

  mPlugins.AppendElement(gmp);
}

void
GeckoMediaPluginService::RemoveOnGMPThread(const nsAString& aDirectory)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = NS_NewLocalFile(aDirectory, false, getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  for (uint32_t i = 0; i < mPlugins.Length(); ++i) {
    nsCOMPtr<nsIFile> pluginpath = mPlugins[i]->GetDirectory();
    bool equals;
    if (NS_SUCCEEDED(directory->Equals(pluginpath, &equals)) && equals) {
      mPlugins[i]->UnloadProcess();
      mPlugins.RemoveElementAt(i);
      return;
    }
  }
  NS_WARNING("Removing GMP which was never added.");
  nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  cs->LogStringMessage(MOZ_UTF16("Removing GMP which was never added."));
}

} // namespace gmp
} // namespace mozilla

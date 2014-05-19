/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "nsIObserverService.h"
#include "GeckoChildProcessHost.h"
#if defined(XP_WIN)
#include "nsIWindowsRegKey.h"
#endif
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXPCOMPrivate.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Services.h"

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

  // Cache user directory while we're on the main thread. We do this because
  // if we try to use "~" in a path during plugin lookup on a non-main thread,
  // the nsIFile code will try to resolve it using NS_GetSpecialDirectory, which
  // doesn't work on non-main threads.
  nsCOMPtr<nsIFile> homeDir;
  NS_GetSpecialDirectory(NS_OS_HOME_DIR, getter_AddRefs(homeDir));
  if (homeDir) {
    homeDir->GetPath(mHomePath);
  }

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
  }

  NS_ADDREF(mGMPThread);
  *aThread = mGMPThread;

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoDecoderVP8(GMPVideoHost** aOutVideoHost, GMPVideoDecoder** aGMPVD)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(NS_LITERAL_CSTRING("decode-video"),
                                               NS_LITERAL_CSTRING("vp8"));
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
GeckoMediaPluginService::GetGMPVideoEncoderVP8(GMPVideoHost** aOutVideoHost, GMPVideoEncoder** aGMPVE)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<GMPParent> gmp = SelectPluginForAPI(NS_LITERAL_CSTRING("encode-video"),
                                               NS_LITERAL_CSTRING("vp8"));
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

GMPParent*
GeckoMediaPluginService::SelectPluginForAPI(const nsCString& aAPI,
                                            const nsCString& aTag)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  GMPParent* gmp = SelectPluginFromListForAPI(aAPI, aTag);
  if (gmp) {
    return gmp;
  }

  RefreshPluginList();

  return SelectPluginFromListForAPI(aAPI, aTag);
}

GMPParent*
GeckoMediaPluginService::SelectPluginFromListForAPI(const nsCString& aAPI,
                                                    const nsCString& aTag)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  for (uint32_t i = 0; i < mPlugins.Length(); i++) {
    GMPParent* gmp = mPlugins[i];
    if (gmp->SupportsAPI(aAPI, aTag)) {
      return gmp;
    }
  }
  return nullptr;
}

nsresult
GeckoMediaPluginService::GetDirectoriesToSearch(nsTArray<nsCOMPtr<nsIFile>> &aDirs)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

#if defined(XP_MACOSX)
  nsCOMPtr<nsIFile> searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  MOZ_ASSERT(!mHomePath.IsEmpty());
  nsresult rv = searchDir->InitWithPath(mHomePath + NS_LITERAL_STRING("/Library/Internet Plug-Ins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
  searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  rv = searchDir->InitWithPath(NS_LITERAL_STRING("/Library/Internet Plug-Ins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
#elif defined(OS_POSIX)
  nsCOMPtr<nsIFile> searchDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  nsresult rv = searchDir->InitWithPath(NS_LITERAL_STRING("/usr/lib/mozilla/plugins/"));
  if (NS_FAILED(rv)) {
    return rv;
  }
  aDirs.AppendElement(searchDir);
#endif
  return NS_OK;
}

#if defined(XP_WIN)
static nsresult
GetPossiblePluginsForRegRoot(uint32_t aKey, nsTArray<nsCOMPtr<nsIFile>>& aDirs)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = regKey->Open(aKey,
                             NS_LITERAL_STRING("Software\\MozillaPlugins"),
                             nsIWindowsRegKey::ACCESS_READ);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t childCount = 0;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(regKey->GetChildCount(&childCount)));
  for (uint32_t index = 0; index < childCount; index++) {
    nsAutoString childName;
    rv = regKey->GetChildName(index, childName);
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsIWindowsRegKey> childKey;
    rv = regKey->OpenChild(childName, nsIWindowsRegKey::ACCESS_QUERY_VALUE,
                           getter_AddRefs(childKey));
    if (NS_FAILED(rv) || !childKey) {
      continue;
    }

    nsAutoString path;
    rv = childKey->ReadStringValue(NS_LITERAL_STRING("Path"), path);
    if (NS_FAILED(rv)) {
      continue;
    }

    nsCOMPtr<nsIFile> localFile;
    if (NS_SUCCEEDED(NS_NewLocalFile(path, true, getter_AddRefs(localFile))) &&
        localFile) {
      bool isFileThere = false;
      if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
        aDirs.AppendElement(localFile);
      }
    }
  }

  regKey->Close();

  return NS_OK;
}
#endif

nsresult
GeckoMediaPluginService::GetPossiblePlugins(nsTArray<nsCOMPtr<nsIFile>>& aDirs)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

#if defined(XP_WIN)
  // The ROOT_KEY_CURRENT_USER entry typically fails to open, causing this call to
  // fail. Don't check any return values because if we find nothing we don't care.
  GetPossiblePluginsForRegRoot(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, aDirs);
  GetPossiblePluginsForRegRoot(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, aDirs);
#endif
  return NS_OK;
}

nsresult
GeckoMediaPluginService::SearchDirectory(nsIFile* aSearchDir)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  MOZ_ASSERT(aSearchDir);

  nsCOMPtr<nsISimpleEnumerator> iter;
  nsresult rv = aSearchDir->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    return rv;
  }

  bool hasMore;
  while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = iter->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) {
      continue;
    }
    nsCOMPtr<nsIFile> dirEntry(do_QueryInterface(supports, &rv));
    if (NS_FAILED(rv)) {
      continue;
    }
    ProcessPossiblePlugin(dirEntry);
  }

  return NS_OK;
}

void
GeckoMediaPluginService::ProcessPossiblePlugin(nsIFile* aDir)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  MOZ_ASSERT(aDir);

  bool isDirectory = false;
  nsresult rv = aDir->IsDirectory(&isDirectory);
  if (NS_FAILED(rv) || !isDirectory) {
    return;
  }

  nsAutoString leafName;
  rv = aDir->GetLeafName(leafName);
  if (NS_FAILED(rv)) {
    return;
  }

  NS_NAMED_LITERAL_STRING(prefix, "gmp-");
  if (leafName.Length() <= prefix.Length() ||
      !Substring(leafName, 0, prefix.Length()).Equals(prefix)) {
    return;
  }

  nsRefPtr<GMPParent> gmp = new GMPParent();
  rv = gmp->Init(aDir);
  if (NS_FAILED(rv)) {
    return;
  }

  mPlugins.AppendElement(gmp);
}

void
GeckoMediaPluginService::RefreshPluginList()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  for (uint32_t iPlusOne = mPlugins.Length(); iPlusOne > 0; iPlusOne--) {
    if (mPlugins[iPlusOne - 1]->State() == GMPStateNotLoaded) {
      mPlugins.RemoveElementAt(iPlusOne - 1);
    }
  }

  nsTArray<nsCOMPtr<nsIFile>> searchDirs;
  nsresult rv = GetDirectoriesToSearch(searchDirs);
  if (NS_FAILED(rv)) {
    return;
  }

  for (uint32_t i = 0; i < searchDirs.Length(); i++) {
    SearchDirectory(searchDirs[i]);
  }

  nsTArray<nsCOMPtr<nsIFile>> possiblePlugins;
  rv = GetPossiblePlugins(possiblePlugins);
  if (NS_FAILED(rv)) {
    return;
  }

  for (uint32_t i = 0; i < possiblePlugins.Length(); i++) {
    ProcessPossiblePlugin(possiblePlugins[i]);
  }
}

} // namespace gmp
} // namespace mozilla

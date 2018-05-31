/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/net/MozURL.h"

#include "nsIEventTarget.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsIServiceWorkerManager.h"

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozJSComponentLoader.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "ServiceWorkerUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

namespace {

static const char* gSupportedRegistrarVersions[] = {
  SERVICEWORKERREGISTRAR_VERSION,
  "7",
  "6",
  "5",
  "4",
  "3",
  "2"
};

static const uint32_t kInvalidGeneration = static_cast<uint32_t>(-1);

StaticRefPtr<ServiceWorkerRegistrar> gServiceWorkerRegistrar;

nsresult
GetOrigin(const nsACString& aURL, nsACString& aOrigin)
{
  RefPtr<net::MozURL> url;
  nsresult rv = net::MozURL::Init(getter_AddRefs(url), aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  url->Origin(aOrigin);
  return NS_OK;
}

nsresult
ReadLine(nsILineInputStream* aStream, nsACString& aValue)
{
  bool hasMoreLines;
  nsresult rv = aStream->ReadLine(aValue, &hasMoreLines);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasMoreLines)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
CreatePrincipalInfo(nsILineInputStream* aStream,
                    ServiceWorkerRegistrationData* aEntry,
                    bool aSkipSpec = false)
{
  nsAutoCString suffix;
  nsresult rv = ReadLine(aStream, suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(suffix)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aSkipSpec) {
    nsAutoCString unused;
    nsresult rv = ReadLine(aStream, unused);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = ReadLine(aStream, aEntry->scope());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  rv = GetOrigin(aEntry->scope(), origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aEntry->principal() =
    mozilla::ipc::ContentPrincipalInfo(attrs, origin, aEntry->scope());

  return NS_OK;
}

} // namespace

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrar,
                  nsIObserver,
                  nsIAsyncShutdownBlocker)

void
ServiceWorkerRegistrar::Initialize()
{
  MOZ_ASSERT(!gServiceWorkerRegistrar);

  if (!XRE_IsParentProcess()) {
    return;
  }

  gServiceWorkerRegistrar = new ServiceWorkerRegistrar();
  ClearOnShutdown(&gServiceWorkerRegistrar);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    DebugOnly<nsresult> rv = obs->AddObserver(gServiceWorkerRegistrar,
                                              "profile-after-change", false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

/* static */ already_AddRefed<ServiceWorkerRegistrar>
ServiceWorkerRegistrar::Get()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_ASSERT(gServiceWorkerRegistrar);
  RefPtr<ServiceWorkerRegistrar> service = gServiceWorkerRegistrar.get();
  return service.forget();
}

ServiceWorkerRegistrar::ServiceWorkerRegistrar()
  : mMonitor("ServiceWorkerRegistrar.mMonitor")
  , mDataLoaded(false)
  , mDataGeneration(kInvalidGeneration)
  , mFileGeneration(kInvalidGeneration)
  , mRetryCount(0)
  , mShuttingDown(false)
  , mRunnableDispatched(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

ServiceWorkerRegistrar::~ServiceWorkerRegistrar()
{
  MOZ_ASSERT(!mRunnableDispatched);
}

void
ServiceWorkerRegistrar::GetRegistrations(
                               nsTArray<ServiceWorkerRegistrationData>& aValues)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aValues.IsEmpty());

  MonitorAutoLock lock(mMonitor);

  // If we don't have the profile directory, profile is not started yet (and
  // probably we are in a utest).
  if (!mProfileDir) {
    return;
  }

  // We care just about the first execution because this can be blocked by
  // loading data from disk.
  static bool firstTime = true;
  TimeStamp startTime;

  if (firstTime) {
    startTime = TimeStamp::NowLoRes();
  }

  // Waiting for data loaded.
  mMonitor.AssertCurrentThreadOwns();
  while (!mDataLoaded) {
    mMonitor.Wait();
  }

  aValues.AppendElements(mData);

  MaybeResetGeneration();
  MOZ_DIAGNOSTIC_ASSERT(mDataGeneration != kInvalidGeneration);
  MOZ_DIAGNOSTIC_ASSERT(mFileGeneration != kInvalidGeneration);

  if (firstTime) {
    firstTime = false;
    Telemetry::AccumulateTimeDelta(
      Telemetry::SERVICE_WORKER_REGISTRATION_LOADING,
      startTime);
  }
}

namespace {

bool Equivalent(const ServiceWorkerRegistrationData& aLeft,
                const ServiceWorkerRegistrationData& aRight)
{
  MOZ_ASSERT(aLeft.principal().type() ==
             mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
  MOZ_ASSERT(aRight.principal().type() ==
             mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

  const auto& leftPrincipal = aLeft.principal().get_ContentPrincipalInfo();
  const auto& rightPrincipal = aRight.principal().get_ContentPrincipalInfo();

  // Only compare the attributes, not the spec part of the principal.
  // The scope comparison above already covers the origin and codebase
  // principals include the full path in their spec which is not what
  // we want here.
  return aLeft.scope() == aRight.scope() &&
         leftPrincipal.attrs() == rightPrincipal.attrs();
}

} // anonymous namespace

void
ServiceWorkerRegistrar::RegisterServiceWorker(
                                     const ServiceWorkerRegistrationData& aData)
{
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to register a serviceWorker during shutting down.");
    return;
  }

  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);
    RegisterServiceWorkerInternal(aData);
  }

  MaybeScheduleSaveData();
  StorageActivityService::SendActivity(aData.principal());
}

void
ServiceWorkerRegistrar::UnregisterServiceWorker(
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsACString& aScope)
{
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to unregister a serviceWorker during shutting down.");
    return;
  }

  bool deleted = false;

  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);

    ServiceWorkerRegistrationData tmp;
    tmp.principal() = aPrincipalInfo;
    tmp.scope() = aScope;

    for (uint32_t i = 0; i < mData.Length(); ++i) {
      if (Equivalent(tmp, mData[i])) {
        mData.RemoveElementAt(i);
        mDataGeneration = GetNextGeneration();
        deleted = true;
        break;
      }
    }
  }

  if (deleted) {
    MaybeScheduleSaveData();
    StorageActivityService::SendActivity(aPrincipalInfo);
  }
}

void
ServiceWorkerRegistrar::RemoveAll()
{
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to remove all the serviceWorkers during shutting down.");
    return;
  }

  bool deleted = false;

  nsTArray<ServiceWorkerRegistrationData> data;
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);

    // Let's take a copy in order to inform StorageActivityService.
    data = mData;

    deleted = !mData.IsEmpty();
    mData.Clear();

    mDataGeneration = GetNextGeneration();
  }

  if (!deleted) {
    return;
  }

  MaybeScheduleSaveData();

  for (uint32_t i = 0, len = data.Length(); i < len; ++i) {
    StorageActivityService::SendActivity(data[i].principal());
  }
}

void
ServiceWorkerRegistrar::LoadData()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mDataLoaded);

  nsresult rv = ReadData();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    DeleteData();
    // Also if the reading failed we have to notify what is waiting for data.
  }

  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(!mDataLoaded);
  mDataLoaded = true;
  mMonitor.Notify();
}

nsresult
ServiceWorkerRegistrar::ReadData()
{
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);

    if (!mProfileDir) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(stream);
  MOZ_ASSERT(lineInputStream);

  nsAutoCString version;
  bool hasMoreLines;
  rv = lineInputStream->ReadLine(version, &hasMoreLines);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!IsSupportedVersion(version)) {
    nsContentUtils::LogMessageToConsole(nsPrintfCString(
      "Unsupported service worker registrar version: %s", version.get()).get());
    return NS_ERROR_FAILURE;
  }

  nsTArray<ServiceWorkerRegistrationData> tmpData;

  bool overwrite = false;
  bool dedupe = false;
  while (hasMoreLines) {
    ServiceWorkerRegistrationData* entry = tmpData.AppendElement();

#define GET_LINE(x)                                   \
    rv = lineInputStream->ReadLine(x, &hasMoreLines); \
    if (NS_WARN_IF(NS_FAILED(rv))) {                  \
      return rv;                                      \
    }                                                 \
    if (NS_WARN_IF(!hasMoreLines)) {                  \
      return NS_ERROR_FAILURE;                        \
    }

    nsAutoCString line;
    if (version.EqualsLiteral(SERVICEWORKERREGISTRAR_VERSION)) {
      rv = CreatePrincipalInfo(lineInputStream, entry);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      nsAutoCString fetchFlag;
      GET_LINE(fetchFlag);
      if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
          !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
        return NS_ERROR_INVALID_ARG;
      }
      entry->currentWorkerHandlesFetch() =
        fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      nsAutoCString updateViaCache;
      GET_LINE(updateViaCache);
      entry->updateViaCache() = updateViaCache.ToInteger(&rv, 16);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      } else if (entry->updateViaCache() > nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE) {
        return NS_ERROR_INVALID_ARG;
      }

      nsAutoCString installedTimeStr;
      GET_LINE(installedTimeStr);
      int64_t installedTime = installedTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->currentWorkerInstalledTime() = installedTime;

      nsAutoCString activatedTimeStr;
      GET_LINE(activatedTimeStr);
      int64_t activatedTime = activatedTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->currentWorkerActivatedTime() = activatedTime;

      nsAutoCString lastUpdateTimeStr;
      GET_LINE(lastUpdateTimeStr);
      int64_t lastUpdateTime = lastUpdateTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->lastUpdateTime() = lastUpdateTime;
    } else if (version.EqualsLiteral("7")) {
      rv = CreatePrincipalInfo(lineInputStream, entry);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      nsAutoCString fetchFlag;
      GET_LINE(fetchFlag);
      if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
          !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
        return NS_ERROR_INVALID_ARG;
      }
      entry->currentWorkerHandlesFetch() =
        fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      nsAutoCString loadFlags;
      GET_LINE(loadFlags);
      entry->updateViaCache() =
        loadFlags.ToInteger(&rv, 16) == nsIRequest::LOAD_NORMAL
          ? nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL
          : nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsAutoCString installedTimeStr;
      GET_LINE(installedTimeStr);
      int64_t installedTime = installedTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->currentWorkerInstalledTime() = installedTime;

      nsAutoCString activatedTimeStr;
      GET_LINE(activatedTimeStr);
      int64_t activatedTime = activatedTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->currentWorkerActivatedTime() = activatedTime;

      nsAutoCString lastUpdateTimeStr;
      GET_LINE(lastUpdateTimeStr);
      int64_t lastUpdateTime = lastUpdateTimeStr.ToInteger64(&rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      entry->lastUpdateTime() = lastUpdateTime;
    } else if (version.EqualsLiteral("6")) {
      rv = CreatePrincipalInfo(lineInputStream, entry);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      nsAutoCString fetchFlag;
      GET_LINE(fetchFlag);
      if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
          !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
        return NS_ERROR_INVALID_ARG;
      }
      entry->currentWorkerHandlesFetch() =
        fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      nsAutoCString loadFlags;
      GET_LINE(loadFlags);
      entry->updateViaCache() =
        loadFlags.ToInteger(&rv, 16) == nsIRequest::LOAD_NORMAL
          ? nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL
          : nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      entry->currentWorkerInstalledTime() = 0;
      entry->currentWorkerActivatedTime() = 0;
      entry->lastUpdateTime() = 0;
    } else if (version.EqualsLiteral("5")) {
      overwrite = true;
      dedupe = true;

      rv = CreatePrincipalInfo(lineInputStream, entry);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      nsAutoCString fetchFlag;
      GET_LINE(fetchFlag);
      if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
          !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
        return NS_ERROR_INVALID_ARG;
      }
      entry->currentWorkerHandlesFetch() =
        fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      entry->updateViaCache() =
        nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      entry->currentWorkerInstalledTime() = 0;
      entry->currentWorkerActivatedTime() = 0;
      entry->lastUpdateTime() = 0;
    } else if (version.EqualsLiteral("4")) {
      overwrite = true;
      dedupe = true;

      rv = CreatePrincipalInfo(lineInputStream, entry);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      // default handlesFetch flag to Enabled
      entry->currentWorkerHandlesFetch() = true;

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      entry->updateViaCache() =
        nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      entry->currentWorkerInstalledTime() = 0;
      entry->currentWorkerActivatedTime() = 0;
      entry->lastUpdateTime() = 0;
    } else if (version.EqualsLiteral("3")) {
      overwrite = true;
      dedupe = true;

      rv = CreatePrincipalInfo(lineInputStream, entry, true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      GET_LINE(entry->currentWorkerURL());

      // default handlesFetch flag to Enabled
      entry->currentWorkerHandlesFetch() = true;

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      entry->updateViaCache() =
        nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      entry->currentWorkerInstalledTime() = 0;
      entry->currentWorkerActivatedTime() = 0;
      entry->lastUpdateTime() = 0;
    } else if (version.EqualsLiteral("2")) {
      overwrite = true;
      dedupe = true;

      rv = CreatePrincipalInfo(lineInputStream, entry, true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // scriptSpec is no more used in latest version.
      nsAutoCString unused;
      GET_LINE(unused);

      GET_LINE(entry->currentWorkerURL());

      // default handlesFetch flag to Enabled
      entry->currentWorkerHandlesFetch() = true;

      nsAutoCString cacheName;
      GET_LINE(cacheName);
      CopyUTF8toUTF16(cacheName, entry->cacheName());

      // waitingCacheName is no more used in latest version.
      GET_LINE(unused);

      entry->updateViaCache() =
        nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

      entry->currentWorkerInstalledTime() = 0;
      entry->currentWorkerActivatedTime() = 0;
      entry->lastUpdateTime() = 0;
    } else {
      MOZ_ASSERT_UNREACHABLE("Should never get here!");
    }

#undef GET_LINE

    rv = lineInputStream->ReadLine(line, &hasMoreLines);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!line.EqualsLiteral(SERVICEWORKERREGISTRAR_TERMINATOR)) {
      return NS_ERROR_FAILURE;
    }
  }

  stream->Close();

  // XXX: The following code is writing to mData without holding a
  //      monitor lock.  This might be ok since this is currently
  //      only called at startup where we block the main thread
  //      preventing further operation until it completes.  We should
  //      consider better locking here in the future.

  // Copy data over to mData.
  for (uint32_t i = 0; i < tmpData.Length(); ++i) {
    // Older versions could sometimes write out empty, useless entries.
    // Prune those here.
    if (!ServiceWorkerRegistrationDataIsValid(tmpData[i])) {
      continue;
    }

    bool match = false;
    if (dedupe) {
      MOZ_ASSERT(overwrite);
      // If this is an old profile, then we might need to deduplicate.  In
      // theory this can be removed in the future (Bug 1248449)
      for (uint32_t j = 0; j < mData.Length(); ++j) {
        // Use same comparison as RegisterServiceWorker. Scope contains
        // basic origin information.  Combine with any principal attributes.
        if (Equivalent(tmpData[i], mData[j])) {
          // Last match wins, just like legacy loading used to do in
          // the ServiceWorkerManager.
          mData[j] = tmpData[i];
          // Dupe found, so overwrite file with reduced list.
          match = true;
          break;
        }
      }
    } else {
#ifdef DEBUG
      // Otherwise assert no duplications in debug builds.
      for (uint32_t j = 0; j < mData.Length(); ++j) {
        MOZ_ASSERT(!Equivalent(tmpData[i], mData[j]));
      }
#endif
    }
    if (!match) {
      mData.AppendElement(tmpData[i]);
    }
  }

  // Overwrite previous version.
  // Cannot call SaveData directly because gtest uses main-thread.
  if (overwrite && NS_FAILED(WriteData(mData))) {
    NS_WARNING("Failed to write data for the ServiceWorker Registations.");
    DeleteData();
  }

  return NS_OK;
}

void
ServiceWorkerRegistrar::DeleteData()
{
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);
    mData.Clear();

    if (!mProfileDir) {
      return;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsresult rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = file->Remove(false);
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    return;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void
ServiceWorkerRegistrar::RegisterServiceWorkerInternal(const ServiceWorkerRegistrationData& aData)
{
  bool found = false;
  for (uint32_t i = 0, len = mData.Length(); i < len; ++i) {
    if (Equivalent(aData, mData[i])) {
      mData[i] = aData;
      found = true;
      break;
    }
  }

  if (!found) {
    MOZ_ASSERT(ServiceWorkerRegistrationDataIsValid(aData));
    mData.AppendElement(aData);
  }

  mDataGeneration = GetNextGeneration();
}

class ServiceWorkerRegistrarSaveDataRunnable final : public Runnable
{
  nsCOMPtr<nsIEventTarget> mEventTarget;
  const nsTArray<ServiceWorkerRegistrationData> mData;
  const uint32_t mGeneration;

public:
  ServiceWorkerRegistrarSaveDataRunnable(nsTArray<ServiceWorkerRegistrationData>&& aData,
                                         uint32_t aGeneration)
    : Runnable("dom::ServiceWorkerRegistrarSaveDataRunnable")
    , mEventTarget(GetCurrentThreadEventTarget())
    , mData(Move(aData))
    , mGeneration(aGeneration)
  {
    AssertIsOnBackgroundThread();
    MOZ_DIAGNOSTIC_ASSERT(mGeneration != kInvalidGeneration);
  }

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerRegistrar> service = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    uint32_t fileGeneration = kInvalidGeneration;

    if (NS_SUCCEEDED(service->SaveData(mData))) {
      fileGeneration = mGeneration;
    }

    RefPtr<Runnable> runnable =
      NewRunnableMethod<uint32_t>("ServiceWorkerRegistrar::DataSaved",
                                  service,
                                  &ServiceWorkerRegistrar::DataSaved,
                                  fileGeneration);
    MOZ_ALWAYS_SUCCEEDS(
      mEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));

    return NS_OK;
  }
};

void
ServiceWorkerRegistrar::MaybeScheduleSaveData()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  if (mShuttingDown || mRunnableDispatched ||
      mDataGeneration <= mFileGeneration) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  uint32_t generation = kInvalidGeneration;
  nsTArray<ServiceWorkerRegistrationData> data;

  {
    MonitorAutoLock lock(mMonitor);
    generation = mDataGeneration;
    data.AppendElements(mData);
  }

  RefPtr<Runnable> runnable =
    new ServiceWorkerRegistrarSaveDataRunnable(Move(data), generation);
  nsresult rv = target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);

  mRunnableDispatched = true;
}

void
ServiceWorkerRegistrar::ShutdownCompleted()
{
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<nsresult> rv = GetShutdownPhase()->RemoveBlocker(this);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult
ServiceWorkerRegistrar::SaveData(const nsTArray<ServiceWorkerRegistrationData>& aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = WriteData(aData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to write data for the ServiceWorker Registations.");
    // Don't touch the file or in-memory state.  Writing files can
    // sometimes fail due to virus scanning, etc.  We should just leave
    // things as is so the next save operation can pick up any changes
    // without losing data.
  }
  return rv;
}

void
ServiceWorkerRegistrar::DataSaved(uint32_t aFileGeneration)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRunnableDispatched);

  mRunnableDispatched = false;

  // Check for shutdown before possibly triggering any more saves
  // runnables.
  MaybeScheduleShutdownCompleted();
  if (mShuttingDown) {
    return;
  }

  // If we got a valid generation, then the save was successful.
  if (aFileGeneration != kInvalidGeneration) {
    // Update the file generation.  We also check to see if we
    // can reset the generation back to zero if the file and data
    // are now in sync.  This allows us to avoid dealing with wrap
    // around of the generation count.
    mFileGeneration = aFileGeneration;
    MaybeResetGeneration();

    // Successful write resets the retry count.
    mRetryCount = 0;

    // Possibly schedule another save operation if more data
    // has come in while processing this one.
    MaybeScheduleSaveData();

    return;
  }

  // Otherwise, the save failed since the generation is invalid.  We
  // want to retry the save, but only a limited number of times.
  static const uint32_t kMaxRetryCount = 2;
  if (mRetryCount >= kMaxRetryCount) {
    return;
  }

  mRetryCount += 1;
  MaybeScheduleSaveData();
}

void
ServiceWorkerRegistrar::MaybeScheduleShutdownCompleted()
{
  AssertIsOnBackgroundThread();

  if (mRunnableDispatched || !mShuttingDown) {
    return;
  }

  RefPtr<Runnable> runnable =
    NewRunnableMethod("dom::ServiceWorkerRegistrar::ShutdownCompleted",
                      this,
                      &ServiceWorkerRegistrar::ShutdownCompleted);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
}

uint32_t
ServiceWorkerRegistrar::GetNextGeneration()
{
  uint32_t ret = mDataGeneration + 1;
  if (ret == kInvalidGeneration) {
    ret += 1;
  }
  return ret;
}

void
ServiceWorkerRegistrar::MaybeResetGeneration()
{
  if (mDataGeneration != mFileGeneration) {
    return;
  }
  mDataGeneration = mFileGeneration = 0;
}

bool
ServiceWorkerRegistrar::IsSupportedVersion(const nsACString& aVersion) const
{
  uint32_t numVersions = ArrayLength(gSupportedRegistrarVersions);
  for (uint32_t i = 0; i < numVersions; i++) {
    if (aVersion.EqualsASCII(gSupportedRegistrarVersions[i])) {
      return true;
    }
  }
  return false;
}

nsresult
ServiceWorkerRegistrar::WriteData(const nsTArray<ServiceWorkerRegistrationData>& aData)
{
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);

    if (!mProfileDir) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString buffer;
  buffer.AppendLiteral(SERVICEWORKERREGISTRAR_VERSION);
  buffer.Append('\n');

  uint32_t count;
  rv = stream->Write(buffer.Data(), buffer.Length(), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (count != buffer.Length()) {
    return NS_ERROR_UNEXPECTED;
  }

  for (uint32_t i = 0, len = aData.Length(); i < len; ++i) {
    // We have an assertion further up the stack, but as a last
    // resort avoid writing out broken entries here.
    if (!ServiceWorkerRegistrationDataIsValid(aData[i])) {
      continue;
    }

    const mozilla::ipc::PrincipalInfo& info = aData[i].principal();

    MOZ_ASSERT(info.type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

    const mozilla::ipc::ContentPrincipalInfo& cInfo =
      info.get_ContentPrincipalInfo();

    nsAutoCString suffix;
    cInfo.attrs().CreateSuffix(suffix);

    buffer.Truncate();
    buffer.Append(suffix.get());
    buffer.Append('\n');

    buffer.Append(aData[i].scope());
    buffer.Append('\n');

    buffer.Append(aData[i].currentWorkerURL());
    buffer.Append('\n');

    buffer.Append(aData[i].currentWorkerHandlesFetch() ?
                    SERVICEWORKERREGISTRAR_TRUE : SERVICEWORKERREGISTRAR_FALSE);
    buffer.Append('\n');

    buffer.Append(NS_ConvertUTF16toUTF8(aData[i].cacheName()));
    buffer.Append('\n');

    buffer.AppendInt(aData[i].updateViaCache(), 16);
    buffer.Append('\n');
    MOZ_DIAGNOSTIC_ASSERT(
      aData[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS ||
      aData[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL ||
      aData[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE
    );

    static_assert(nsIRequest::LOAD_NORMAL == 0,
                  "LOAD_NORMAL matches serialized value.");
    static_assert(nsIRequest::VALIDATE_ALWAYS == (1 << 11),
                  "VALIDATE_ALWAYS matches serialized value");

    buffer.AppendInt(aData[i].currentWorkerInstalledTime());
    buffer.Append('\n');

    buffer.AppendInt(aData[i].currentWorkerActivatedTime());
    buffer.Append('\n');

    buffer.AppendInt(aData[i].lastUpdateTime());
    buffer.Append('\n');

    buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR);
    buffer.Append('\n');

    rv = stream->Write(buffer.Data(), buffer.Length(), &count);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (count != buffer.Length()) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(stream);
  MOZ_ASSERT(safeStream);

  rv = safeStream->Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
ServiceWorkerRegistrar::ProfileStarted()
{
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);
  MOZ_DIAGNOSTIC_ASSERT(!mProfileDir);

  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mProfileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = GetShutdownPhase()->AddBlocker(
    this, NS_LITERAL_STRING(__FILE__), __LINE__,
    NS_LITERAL_STRING("ServiceWorkerRegistrar: Flushing data"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("dom::ServiceWorkerRegistrar::LoadData",
                      this,
                      &ServiceWorkerRegistrar::LoadData);
  rv = target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
}

void
ServiceWorkerRegistrar::ProfileStopped()
{
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  if (!mProfileDir) {
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(mProfileDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  PBackgroundChild* child = BackgroundChild::GetForCurrentThread();
  if (!child) {
    // Mutations to the ServiceWorkerRegistrar happen on the PBackground thread,
    // issued by the ServiceWorkerManagerService, so the appropriate place to
    // trigger shutdown is on that thread.
    //
    // However, it's quite possible that the PBackground thread was not brought
    // into existence for xpcshell tests.  We don't cause it to be created
    // ourselves for any reason, for example.
    //
    // In this scenario, we know that:
    // - We will receive exactly one call to ourself from BlockShutdown() and
    //   BlockShutdown() will be called (at most) once.
    // - The only way our Shutdown() method gets called is via
    //   BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar() being
    //   invoked, which only happens if we get to that send below here that we
    //   can't get to.
    // - All Shutdown() does is set mShuttingDown=true (essential for
    //   invariants) and invoke MaybeScheduleShutdownCompleted().
    // - Since there is no PBackground thread, mRunnableDispatched must be false
    //   because only MaybeScheduleSaveData() set it and it only runs on the
    //   background thread, so it cannot have run.  And so we would expect
    //   MaybeScheduleShutdownCompleted() to schedule an invocation of
    //   ShutdownCompleted on the main thread.
    //
    // So it's appropriate for us to set mShuttingDown=true (as Shutdown would
    // do) and directly invoke ShutdownCompleted() (as Shutdown would indirectly
    // do via MaybeScheduleShutdownCompleted).
    mShuttingDown = true;
    ShutdownCompleted();
    return;
  }

  child->SendShutdownServiceWorkerRegistrar();
}

// Async shutdown blocker methods

NS_IMETHODIMP
ServiceWorkerRegistrar::BlockShutdown(nsIAsyncShutdownClient* aClient)
{
  ProfileStopped();
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrar::GetName(nsAString& aName)
{
  aName = NS_LITERAL_STRING("ServiceWorkerRegistrar: Flushing data");
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrar::GetState(nsIPropertyBag**)
{
  return NS_OK;
}

#define RELEASE_ASSERT_SUCCEEDED(rv, name) do { \
    if (NS_FAILED(rv)) {                                                       \
      mozJSComponentLoader::Get()->AnnotateCrashReport();                      \
      if (rv == NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS) {                  \
        if (auto* context = CycleCollectedJSContext::Get()) {                  \
          if (RefPtr<Exception> exn = context->GetPendingException()) {        \
            MOZ_CRASH_UNSAFE_PRINTF("Failed to get " name ": %s",              \
                                    exn->GetMessageMoz().get());               \
          }                                                                    \
        }                                                                      \
      }                                                                        \
                                                                               \
      nsAutoCString errorName;                                                 \
      GetErrorName(rv, errorName);                                             \
      MOZ_CRASH_UNSAFE_PRINTF("Failed to get " name ": %s",                    \
                              errorName.get());                                \
    }                                                                          \
  } while (0)


nsCOMPtr<nsIAsyncShutdownClient>
ServiceWorkerRegistrar::GetShutdownPhase() const
{
  nsresult rv;
  nsCOMPtr<nsIAsyncShutdownService> svc = do_GetService(
      "@mozilla.org/async-shutdown-service;1", &rv);
  // If this fails, something is very wrong on the JS side (or we're out of
  // memory), and there's no point in continuing startup. Include as much
  // information as possible in the crash report.
  RELEASE_ASSERT_SUCCEEDED(rv, "async shutdown service");


  nsCOMPtr<nsIAsyncShutdownClient> client;
  rv = svc->GetProfileBeforeChange(getter_AddRefs(client));
  RELEASE_ASSERT_SUCCEEDED(rv, "profileBeforeChange shutdown blocker");
  return Move(client);
}

#undef RELEASE_ASSERT_SUCCEEDED

void
ServiceWorkerRegistrar::Shutdown()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  mShuttingDown = true;
  MaybeScheduleShutdownCompleted();
}

NS_IMETHODIMP
ServiceWorkerRegistrar::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "profile-after-change")) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    observerService->RemoveObserver(this, "profile-after-change");

    // The profile is fully loaded, now we can proceed with the loading of data
    // from disk.
    ProfileStarted();

    return NS_OK;
  }

  MOZ_ASSERT(false, "ServiceWorkerRegistrar got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

} // namespace dom
} // namespace mozilla

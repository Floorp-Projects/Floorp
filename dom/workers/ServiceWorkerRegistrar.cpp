
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"

#include "nsIEventTarget.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

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

StaticRefPtr<ServiceWorkerRegistrar> gServiceWorkerRegistrar;

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
  , mShuttingDown(false)
  , mRunnableCounter(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

ServiceWorkerRegistrar::~ServiceWorkerRegistrar()
{
  MOZ_ASSERT(!mRunnableCounter);
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

  ScheduleSaveData();
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
        deleted = true;
        break;
      }
    }
  }

  if (deleted) {
    ScheduleSaveData();
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

  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);

    deleted = !mData.IsEmpty();
    mData.Clear();
  }

  if (deleted) {
    ScheduleSaveData();
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
    nsAutoCString unused;
    if (version.EqualsLiteral(SERVICEWORKERREGISTRAR_VERSION)) {
      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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
      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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
      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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

      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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

      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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

      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      // principal spec is no longer used; we use scope directly instead
      GET_LINE(unused);

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

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

      nsAutoCString suffix;
      GET_LINE(suffix);

      OriginAttributes attrs;
      if (!attrs.PopulateFromSuffix(suffix)) {
        return NS_ERROR_INVALID_ARG;
      }

      // principal spec is no longer used; we use scope directly instead
      GET_LINE(unused);

      GET_LINE(entry->scope());

      entry->principal() =
        mozilla::ipc::ContentPrincipalInfo(attrs, void_t(), entry->scope());

      // scriptSpec is no more used in latest version.
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

  // Copy data over to mData.
  for (uint32_t i = 0; i < tmpData.Length(); ++i) {
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
  if (overwrite && NS_FAILED(WriteData())) {
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
    mData.AppendElement(aData);
  }
}

class ServiceWorkerRegistrarSaveDataRunnable final : public Runnable
{
public:
  ServiceWorkerRegistrarSaveDataRunnable()
    : Runnable("dom::ServiceWorkerRegistrarSaveDataRunnable")
    , mEventTarget(GetCurrentThreadEventTarget())
  {
    AssertIsOnBackgroundThread();
  }

  NS_IMETHOD
  Run() override
  {
    RefPtr<ServiceWorkerRegistrar> service = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->SaveData();

    RefPtr<Runnable> runnable =
      NewRunnableMethod("ServiceWorkerRegistrar::DataSaved",
                        service, &ServiceWorkerRegistrar::DataSaved);
    nsresult rv = mEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

void
ServiceWorkerRegistrar::ScheduleSaveData()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  RefPtr<Runnable> runnable =
    new ServiceWorkerRegistrarSaveDataRunnable();
  nsresult rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  ++mRunnableCounter;
}

void
ServiceWorkerRegistrar::ShutdownCompleted()
{
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<nsresult> rv = GetShutdownPhase()->RemoveBlocker(this);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void
ServiceWorkerRegistrar::SaveData()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = WriteData();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to write data for the ServiceWorker Registations.");
    DeleteData();
  }
}

void
ServiceWorkerRegistrar::DataSaved()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mRunnableCounter);

  --mRunnableCounter;
  MaybeScheduleShutdownCompleted();
}

void
ServiceWorkerRegistrar::MaybeScheduleShutdownCompleted()
{
  AssertIsOnBackgroundThread();

  if (mRunnableCounter || !mShuttingDown) {
    return;
  }

  RefPtr<Runnable> runnable =
    NewRunnableMethod("dom::ServiceWorkerRegistrar::ShutdownCompleted",
                      this,
                      &ServiceWorkerRegistrar::ShutdownCompleted);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
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
ServiceWorkerRegistrar::WriteData()
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

  // We need a lock to take a snapshot of the data.
  nsTArray<ServiceWorkerRegistrationData> data;
  {
    MonitorAutoLock lock(mMonitor);
    data = mData;
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

  for (uint32_t i = 0, len = data.Length(); i < len; ++i) {
    const mozilla::ipc::PrincipalInfo& info = data[i].principal();

    MOZ_ASSERT(info.type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

    const mozilla::ipc::ContentPrincipalInfo& cInfo =
      info.get_ContentPrincipalInfo();

    nsAutoCString suffix;
    cInfo.attrs().CreateSuffix(suffix);

    buffer.Truncate();
    buffer.Append(suffix.get());
    buffer.Append('\n');

    buffer.Append(data[i].scope());
    buffer.Append('\n');

    buffer.Append(data[i].currentWorkerURL());
    buffer.Append('\n');

    buffer.Append(data[i].currentWorkerHandlesFetch() ?
                    SERVICEWORKERREGISTRAR_TRUE : SERVICEWORKERREGISTRAR_FALSE);
    buffer.Append('\n');

    buffer.Append(NS_ConvertUTF16toUTF8(data[i].cacheName()));
    buffer.Append('\n');

    buffer.AppendInt(data[i].updateViaCache(), 16);
    buffer.Append('\n');
    MOZ_DIAGNOSTIC_ASSERT(
      data[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS ||
      data[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL ||
      data[i].updateViaCache() == nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE
    );

    static_assert(nsIRequest::LOAD_NORMAL == 0,
                  "LOAD_NORMAL matches serialized value.");
    static_assert(nsIRequest::VALIDATE_ALWAYS == (1 << 11),
                  "VALIDATE_ALWAYS matches serialized value");

    buffer.AppendInt(data[i].currentWorkerInstalledTime());
    buffer.Append('\n');

    buffer.AppendInt(data[i].currentWorkerActivatedTime());
    buffer.Append('\n');

    buffer.AppendInt(data[i].lastUpdateTime());
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
  rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
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

nsCOMPtr<nsIAsyncShutdownClient>
ServiceWorkerRegistrar::GetShutdownPhase() const
{
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> client;
  Unused << svc->GetProfileBeforeChange(getter_AddRefs(client));
  MOZ_RELEASE_ASSERT(client);
  return Move(client);
}

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

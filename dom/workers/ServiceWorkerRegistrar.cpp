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

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsAutoPtr.h"
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

StaticRefPtr<ServiceWorkerRegistrar> gServiceWorkerRegistrar;

} // anonymous namespace

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrar,
                  nsIObserver)

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

    rv = obs->AddObserver(gServiceWorkerRegistrar, "profile-before-change",
                          false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

/* static */ already_AddRefed<ServiceWorkerRegistrar>
ServiceWorkerRegistrar::Get()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_ASSERT(gServiceWorkerRegistrar);
  nsRefPtr<ServiceWorkerRegistrar> service = gServiceWorkerRegistrar.get();
  return service.forget();
}

ServiceWorkerRegistrar::ServiceWorkerRegistrar()
  : mMonitor("ServiceWorkerRegistrar.mMonitor")
  , mDataLoaded(false)
  , mShuttingDown(false)
  , mShutdownCompleteFlag(nullptr)
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

  {
    MonitorAutoLock lock(mMonitor);

    // Waiting for data loaded.
    mMonitor.AssertCurrentThreadOwns();
    while (!mDataLoaded) {
      mMonitor.Wait();
    }

    aValues.AppendElements(mData);
  }

  if (firstTime) {
    firstTime = false;
    Telemetry::AccumulateTimeDelta(
      Telemetry::SERVICE_WORKER_REGISTRATION_LOADING,
      startTime);
  }
}

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

    const mozilla::ipc::PrincipalInfo& newPrincipalInfo = aData.principal();
    MOZ_ASSERT(newPrincipalInfo.type() ==
               mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

    const mozilla::ipc::ContentPrincipalInfo& newContentPrincipalInfo =
      newPrincipalInfo.get_ContentPrincipalInfo();

    bool found = false;
    for (uint32_t i = 0, len = mData.Length(); i < len; ++i) {
      if (mData[i].scope() == aData.scope()) {
        const mozilla::ipc::PrincipalInfo& existingPrincipalInfo =
          mData[i].principal();
        const mozilla::ipc::ContentPrincipalInfo& existingContentPrincipalInfo =
          existingPrincipalInfo.get_ContentPrincipalInfo();

        if (newContentPrincipalInfo == existingContentPrincipalInfo) {
          mData[i] = aData;
          found = true;
          break;
        }
      }
    }

    if (!found) {
      mData.AppendElement(aData);
    }
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

    for (uint32_t i = 0; i < mData.Length(); ++i) {
      if (mData[i].principal() == aPrincipalInfo &&
          mData[i].scope() == aScope) {
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

  MOZ_ASSERT(mProfileDir);

  nsCOMPtr<nsIFile> file;
  nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
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

  nsAutoCString line;
  bool hasMoreLines;
  rv = lineInputStream->ReadLine(line, &hasMoreLines);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The file is corrupted ?
  // FIXME: in case we implement a version 2, we should inform the user using
  // the console about this issue.
  if (!line.EqualsLiteral(SERVICEWORKERREGISTRAR_VERSION)) {
    return NS_ERROR_FAILURE;
  }

  while (hasMoreLines) {
    ServiceWorkerRegistrationData* entry = mData.AppendElement();

#define GET_LINE(x)                                   \
    rv = lineInputStream->ReadLine(x, &hasMoreLines); \
    if (NS_WARN_IF(NS_FAILED(rv))) {                  \
      return rv;                                      \
    }                                                 \
    if (NS_WARN_IF(!hasMoreLines)) {                  \
      return NS_ERROR_FAILURE;                        \
    }

    GET_LINE(line);

    uint32_t appId = line.ToInteger(&rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    GET_LINE(line);

    if (!line.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
        !line.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
      return NS_ERROR_FAILURE;
    }

    bool isInBrowserElement = line.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

    GET_LINE(line);
    entry->principal() =
      mozilla::ipc::ContentPrincipalInfo(appId, isInBrowserElement, line);

    GET_LINE(entry->scope());
    GET_LINE(entry->scriptSpec());
    GET_LINE(entry->currentWorkerURL());

    nsAutoCString cacheName;
    GET_LINE(cacheName);
    CopyUTF8toUTF16(cacheName, entry->activeCacheName());

    GET_LINE(cacheName);
    CopyUTF8toUTF16(cacheName, entry->waitingCacheName());

#undef GET_LINE

    rv = lineInputStream->ReadLine(line, &hasMoreLines);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!line.EqualsLiteral(SERVICEWORKERREGISTRAR_TERMINATOR)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

void
ServiceWorkerRegistrar::DeleteData()
{
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  MOZ_ASSERT(mProfileDir);

  {
    MonitorAutoLock lock(mMonitor);
    mData.Clear();
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
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

class ServiceWorkerRegistrarSaveDataRunnable final : public nsRunnable
{
public:
  ServiceWorkerRegistrarSaveDataRunnable()
    : mThread(do_GetCurrentThread())
  {
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    nsRefPtr<ServiceWorkerRegistrar> service = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->SaveData();

    nsRefPtr<nsRunnable> runnable =
      NS_NewRunnableMethod(service, &ServiceWorkerRegistrar::DataSaved);
    nsresult rv = mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

void
ServiceWorkerRegistrar::ScheduleSaveData()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  nsRefPtr<nsRunnable> runnable =
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

  MOZ_ASSERT(mShutdownCompleteFlag && !*mShutdownCompleteFlag);
  *mShutdownCompleteFlag = true;
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

  nsRefPtr<nsRunnable> runnable =
     NS_NewRunnableMethod(this, &ServiceWorkerRegistrar::ShutdownCompleted);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

nsresult
ServiceWorkerRegistrar::WriteData()
{
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  MOZ_ASSERT(mProfileDir);

  nsCOMPtr<nsIFile> file;
  nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
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

    buffer.Truncate();
    buffer.AppendInt(cInfo.appId());
    buffer.Append('\n');

    if (cInfo.isInBrowserElement()) {
      buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TRUE);
    } else {
      buffer.AppendLiteral(SERVICEWORKERREGISTRAR_FALSE);
    }

    buffer.Append('\n');
    buffer.Append(cInfo.spec());

    buffer.Append('\n');

    buffer.Append(data[i].scope());
    buffer.Append('\n');

    buffer.Append(data[i].scriptSpec());
    buffer.Append('\n');

    buffer.Append(data[i].currentWorkerURL());
    buffer.Append('\n');

    buffer.Append(NS_ConvertUTF16toUTF8(data[i].activeCacheName()));
    buffer.Append('\n');

    buffer.Append(NS_ConvertUTF16toUTF8(data[i].waitingCacheName()));
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
  MOZ_ASSERT(!mProfileDir);

  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mProfileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &ServiceWorkerRegistrar::LoadData);
  rv = target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
}

void
ServiceWorkerRegistrar::ProfileStopped()
{
  MOZ_ASSERT(NS_IsMainThread());

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

  bool completed = false;
  mShutdownCompleteFlag = &completed;

  child->SendShutdownServiceWorkerRegistrar();

  nsCOMPtr<nsIThread> thread(do_GetCurrentThread());
  while (true) {
    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(thread));
    if (completed) {
      break;
    }
  }
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

  if (!strcmp(aTopic, "profile-before-change")) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    observerService->RemoveObserver(this, "profile-before-change");

    // Shutting down, let's sync the data.
    ProfileStopped();

    return NS_OK;
  }

  MOZ_ASSERT(false, "ServiceWorkerRegistrar got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

} // dom namespace
} // mozilla namespace

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistrar_h
#define mozilla_dom_ServiceWorkerRegistrar_h

#include "mozilla/Monitor.h"
#include "mozilla/Telemetry.h"
#include "nsClassHashtable.h"
#include "nsIAsyncShutdown.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

#define SERVICEWORKERREGISTRAR_FILE u"serviceworker.txt"
#define SERVICEWORKERREGISTRAR_VERSION "9"
#define SERVICEWORKERREGISTRAR_TERMINATOR "#"
#define SERVICEWORKERREGISTRAR_TRUE "true"
#define SERVICEWORKERREGISTRAR_FALSE "false"

class nsIFile;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

class ServiceWorkerRegistrationData;
}
}  // namespace mozilla

namespace mozilla::dom {

class ServiceWorkerRegistrar : public nsIObserver,
                               public nsIAsyncShutdownBlocker {
  friend class ServiceWorkerRegistrarSaveDataRunnable;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  static void Initialize();

  void Shutdown();

  void DataSaved(uint32_t aFileGeneration);

  static already_AddRefed<ServiceWorkerRegistrar> Get();

  void GetRegistrations(nsTArray<ServiceWorkerRegistrationData>& aValues);

  void RegisterServiceWorker(const ServiceWorkerRegistrationData& aData);
  void UnregisterServiceWorker(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
      const nsACString& aScope);
  void RemoveAll();

  bool ReloadDataForTest();

 protected:
  // These methods are protected because we test this class using gTest
  // subclassing it.
  void LoadData();
  nsresult SaveData(const nsTArray<ServiceWorkerRegistrationData>& aData);

  nsresult ReadData();
  nsresult WriteData(const nsTArray<ServiceWorkerRegistrationData>& aData);
  void DeleteData();

  void RegisterServiceWorkerInternal(const ServiceWorkerRegistrationData& aData)
      MOZ_REQUIRES(mMonitor);

  ServiceWorkerRegistrar();
  virtual ~ServiceWorkerRegistrar();

 private:
  void ProfileStarted();
  void ProfileStopped();

  void MaybeScheduleSaveData();
  void ShutdownCompleted();
  void MaybeScheduleShutdownCompleted();

  uint32_t GetNextGeneration();
  void MaybeResetGeneration();

  nsCOMPtr<nsIAsyncShutdownClient> GetShutdownPhase() const;

  bool IsSupportedVersion(const nsACString& aVersion) const;

 protected:
  mozilla::Monitor mMonitor;

  // protected by mMonitor.
  nsCOMPtr<nsIFile> mProfileDir MOZ_GUARDED_BY(mMonitor);
  // Read on mainthread, modified on background thread EXCEPT for
  // ReloadDataForTest() AND for gtest, which modifies this on MainThread.
  nsTArray<ServiceWorkerRegistrationData> mData MOZ_GUARDED_BY(mMonitor);
  bool mDataLoaded MOZ_GUARDED_BY(mMonitor);

  // PBackground thread only
  uint32_t mDataGeneration;
  uint32_t mFileGeneration;
  uint32_t mRetryCount;
  bool mShuttingDown;
  bool mSaveDataRunnableDispatched;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ServiceWorkerRegistrar_h

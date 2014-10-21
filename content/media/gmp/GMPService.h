/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPService_h_
#define GMPService_h_

#include "nsString.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "mozilla/Atomics.h"

template <class> struct already_AddRefed;

namespace mozilla {
namespace gmp {

class GMPParent;

class GeckoMediaPluginService MOZ_FINAL : public mozIGeckoMediaPluginService
                                        , public nsIObserver
{
public:
  static already_AddRefed<GeckoMediaPluginService> GetGeckoMediaPluginService();

  GeckoMediaPluginService();
  nsresult Init();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZIGECKOMEDIAPLUGINSERVICE
  NS_DECL_NSIOBSERVER

  void AsyncShutdownNeeded(GMPParent* aParent);
  void AsyncShutdownComplete(GMPParent* aParent);
  void AbortAsyncShutdown();

private:
  ~GeckoMediaPluginService();

  void ClearStorage();

  GMPParent* SelectPluginForAPI(const nsACString& aNodeId,
                                const nsCString& aAPI,
                                const nsTArray<nsCString>& aTags);
  GMPParent* FindPluginForAPIFrom(size_t aSearchStartIndex,
                                  const nsCString& aAPI,
                                  const nsTArray<nsCString>& aTags,
                                  size_t* aOutPluginIndex);

  void UnloadPlugins();
  void CrashPlugins();
  void SetAsyncShutdownComplete();

  void LoadFromEnvironment();
  void ProcessPossiblePlugin(nsIFile* aDir);

  void AddOnGMPThread(const nsAString& aSearchDir);
  void RemoveOnGMPThread(const nsAString& aSearchDir);

  nsresult SetAsyncShutdownTimeout();

protected:
  friend class GMPParent;
  void ReAddOnGMPThread(nsRefPtr<GMPParent>& aOld);
private:
  GMPParent* ClonePlugin(const GMPParent* aOriginal);

  class PathRunnable : public nsRunnable
  {
  public:
    PathRunnable(GeckoMediaPluginService* service, const nsAString& path,
                 bool add)
      : mService(service)
      , mPath(path)
      , mAdd(add)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsRefPtr<GeckoMediaPluginService> mService;
    nsString mPath;
    bool mAdd;
  };

  Mutex mMutex; // Protects mGMPThread and mShuttingDown and mPlugins
  nsTArray<nsRefPtr<GMPParent>> mPlugins;
  nsCOMPtr<nsIThread> mGMPThread;
  bool mShuttingDown;
  bool mShuttingDownOnGMPThread;

  // True if we've inspected MOZ_GMP_PATH on the GMP thread and loaded any
  // plugins found there into mPlugins.
  Atomic<bool> mScannedPluginOnDisk;

  template<typename T>
  class MainThreadOnly {
  public:
    MOZ_IMPLICIT MainThreadOnly(T aValue)
      : mValue(aValue)
    {}
    operator T&() {
      MOZ_ASSERT(NS_IsMainThread());
      return mValue;
    }

  private:
    T mValue;
  };

  MainThreadOnly<bool> mWaitingForPluginsAsyncShutdown;

  nsTArray<nsRefPtr<GMPParent>> mAsyncShutdownPlugins; // GMP Thread only.
  nsCOMPtr<nsITimer> mAsyncShutdownTimeout; // GMP Thread only.

#ifndef MOZ_WIDGET_GONK
  nsCOMPtr<nsIFile> mStorageBaseDir;
#endif

  // Hashes of (origin,topLevelOrigin) to the node id for
  // non-persistent sessions.
  nsClassHashtable<nsUint32HashKey, nsCString> mTempNodeIds;

  // Hashes node id to whether that node id is allowed to store data
  // persistently on disk.
  nsDataHashtable<nsCStringHashKey, bool> mPersistentStorageAllowed;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPService_h_

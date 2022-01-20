/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPServiceParent_h_
#define GMPServiceParent_h_

#include "GMPService.h"
#include "mozilla/gmp/PGMPServiceParent.h"
#include "mozIGeckoMediaPluginChromeService.h"
#include "nsClassHashtable.h"
#include "nsTHashMap.h"
#include "mozilla/Atomics.h"
#include "nsNetUtil.h"
#include "nsIAsyncShutdown.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "mozilla/MozPromise.h"
#include "GMPStorage.h"

template <class>
struct already_AddRefed;

namespace mozilla {
class OriginAttributesPattern;

namespace gmp {

class GMPParent;
class GMPServiceParent;

class GeckoMediaPluginServiceParent final
    : public GeckoMediaPluginService,
      public mozIGeckoMediaPluginChromeService,
      public nsIAsyncShutdownBlocker {
 public:
  static already_AddRefed<GeckoMediaPluginServiceParent> GetSingleton();

  GeckoMediaPluginServiceParent();
  nsresult Init() override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  // mozIGeckoMediaPluginService
  NS_IMETHOD HasPluginForAPI(const nsACString& aAPI, nsTArray<nsCString>* aTags,
                             bool* aRetVal) override;
  NS_IMETHOD GetNodeId(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aGMPName,
                       UniquePtr<GetNodeIdCallback>&& aCallback) override;

  NS_DECL_MOZIGECKOMEDIAPLUGINCHROMESERVICE
  NS_DECL_NSIOBSERVER

  RefPtr<GenericPromise> EnsureInitialized();
  RefPtr<GenericPromise> AsyncAddPluginDirectory(const nsAString& aDirectory);

  // GMP thread access only
  bool IsShuttingDown();

  already_AddRefed<GMPStorage> GetMemoryStorageFor(const nsACString& aNodeId);
  nsresult ForgetThisSiteNative(
      const nsAString& aSite, const mozilla::OriginAttributesPattern& aPattern);

  nsresult ForgetThisBaseDomainNative(const nsAString& aBaseDomain);

  // Notifies that some user of this class is created/destroyed.
  void ServiceUserCreated(GMPServiceParent* aServiceParent);
  void ServiceUserDestroyed(GMPServiceParent* aServiceParent);

  void UpdateContentProcessGMPCapabilities();

 private:
  friend class GMPServiceParent;

  virtual ~GeckoMediaPluginServiceParent();

  void ClearStorage();

  already_AddRefed<GMPParent> SelectPluginForAPI(
      const nsACString& aNodeId, const nsCString& aAPI,
      const nsTArray<nsCString>& aTags);

  already_AddRefed<GMPParent> FindPluginForAPIFrom(
      size_t aSearchStartIndex, const nsCString& aAPI,
      const nsTArray<nsCString>& aTags, size_t* aOutPluginIndex);

  nsresult GetNodeId(const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
                     const nsAString& aGMPName, nsACString& aOutId);

  void UnloadPlugins();
  void CrashPlugins();
  void NotifySyncShutdownComplete();

  void RemoveOnGMPThread(const nsAString& aDirectory,
                         const bool aDeleteFromDisk, const bool aCanDefer);

  struct DirectoryFilter {
    virtual bool operator()(nsIFile* aPath) = 0;
    ~DirectoryFilter() = default;
  };
  void ClearNodeIdAndPlugin(DirectoryFilter& aFilter);
  void ClearNodeIdAndPlugin(nsIFile* aPluginStorageDir,
                            DirectoryFilter& aFilter);
  void ForgetThisSiteOnGMPThread(
      const nsACString& aSite,
      const mozilla::OriginAttributesPattern& aPattern);
  void ForgetThisBaseDomainOnGMPThread(const nsACString& aBaseDomain);
  void ClearRecentHistoryOnGMPThread(PRTime aSince);

  already_AddRefed<GMPParent> GetById(uint32_t aPluginId);

 protected:
  friend class GMPParent;
  void ReAddOnGMPThread(const RefPtr<GMPParent>& aOld);
  void PluginTerminated(const RefPtr<GMPParent>& aOld);
  void InitializePlugins(nsISerialEventTarget* GMPThread) override;
  RefPtr<GenericPromise> LoadFromEnvironment();
  RefPtr<GenericPromise> AddOnGMPThread(nsString aDirectory);

  RefPtr<GetGMPContentParentPromise> GetContentParent(
      GMPCrashHelper* aHelper, const NodeIdVariant& aNodeIdVariant,
      const nsCString& aAPI, const nsTArray<nsCString>& aTags) override;

 private:
  // Creates a copy of aOriginal. Note that the caller is responsible for
  // adding this to GeckoMediaPluginServiceParent::mPlugins.
  already_AddRefed<GMPParent> ClonePlugin(const GMPParent* aOriginal);
  nsresult EnsurePluginsOnDiskScanned();
  nsresult InitStorage();

  // Get a string based node ID from a NodeIdVariant. This will
  // either fetch the internal string, or convert the internal NodeIdParts to a
  // string. The conversion process is fallible, so the return value should be
  // checked.
  nsresult GetNodeId(const NodeIdVariant& aNodeIdVariant, nsACString& aOutId);

  class PathRunnable : public Runnable {
   public:
    enum EOperation {
      REMOVE,
      REMOVE_AND_DELETE_FROM_DISK,
    };

    PathRunnable(GeckoMediaPluginServiceParent* aService,
                 const nsAString& aPath, EOperation aOperation,
                 bool aDefer = false)
        : Runnable("gmp::GeckoMediaPluginServiceParent::PathRunnable"),
          mService(aService),
          mPath(aPath),
          mOperation(aOperation),
          mDefer(aDefer) {}

    NS_DECL_NSIRUNNABLE

   private:
    RefPtr<GeckoMediaPluginServiceParent> mService;
    nsString mPath;
    EOperation mOperation;
    bool mDefer;
  };

  // Protected by mMutex from the base class.
  nsTArray<RefPtr<GMPParent>> mPlugins;
  bool mShuttingDown;

  // True if we've inspected MOZ_GMP_PATH on the GMP thread and loaded any
  // plugins found there into mPlugins.
  Atomic<bool> mScannedPluginOnDisk;

  template <typename T>
  class MainThreadOnly {
   public:
    MOZ_IMPLICIT MainThreadOnly(T aValue) : mValue(aValue) {}
    operator T&() {
      MOZ_ASSERT(NS_IsMainThread());
      return mValue;
    }

   private:
    T mValue;
  };

  MainThreadOnly<bool> mWaitingForPluginsSyncShutdown;

  nsTArray<nsString> mPluginsWaitingForDeletion;

  nsCOMPtr<nsIFile> mStorageBaseDir;

  // Hashes of (origin,topLevelOrigin) to the node id for
  // non-persistent sessions.
  nsClassHashtable<nsUint32HashKey, nsCString> mTempNodeIds;

  // Hashes node id to whether that node id is allowed to store data
  // persistently on disk.
  nsTHashMap<nsCStringHashKey, bool> mPersistentStorageAllowed;

  // Synchronization for barrier that ensures we've loaded GMPs from
  // MOZ_GMP_PATH before allowing GetContentParentFrom() to proceed.
  Monitor mInitPromiseMonitor;
  MozMonitoredPromiseHolder<GenericPromise> mInitPromise;
  bool mLoadPluginsFromDiskComplete;

  // Hashes nodeId to the hashtable of storage for that nodeId.
  nsRefPtrHashtable<nsCStringHashKey, GMPStorage> mTempGMPStorage;

  // Tracks how many IPC connections to GMPServices running in content
  // processes we have. When this is empty we can safely shut down.
  // Synchronized across thread via mMutex in base class.
  nsTArray<GMPServiceParent*> mServiceParents;
};

nsresult WriteToFile(nsIFile* aPath, const nsCString& aFileName,
                     const nsCString& aData);
nsresult ReadSalt(nsIFile* aPath, nsACString& aOutData);
bool MatchOrigin(nsIFile* aPath, const nsACString& aSite,
                 const mozilla::OriginAttributesPattern& aPattern);
bool MatchBaseDomain(nsIFile* aPath, const nsACString& aBaseDomain);

class GMPServiceParent final : public PGMPServiceParent {
 public:
  explicit GMPServiceParent(GeckoMediaPluginServiceParent* aService);

  // Our refcounting is thread safe, and when our refcount drops to zero
  // we dispatch an event to the main thread to delete the GMPServiceParent.
  // Note that this means it's safe for references to this object to be
  // released on a non main thread, but the destructor will always run on
  // the main thread.

  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PGMPServiceParent.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(
      GMPServiceParent, final);

  ipc::IPCResult RecvGetGMPNodeId(const nsString& aOrigin,
                                  const nsString& aTopLevelOrigin,
                                  const nsString& aGMPName,
                                  nsCString* aID) override;

  static bool Create(Endpoint<PGMPServiceParent>&& aGMPService);

  ipc::IPCResult RecvLaunchGMP(
      const NodeIdVariant& aNodeIdVariant, const nsCString& aAPI,
      nsTArray<nsCString>&& aTags, nsTArray<ProcessId>&& aAlreadyBridgedTo,
      uint32_t* aOutPluginId, ProcessId* aOutProcessId,
      nsCString* aOutDisplayName, Endpoint<PGMPContentParent>* aOutEndpoint,
      nsresult* aOutRv, nsCString* aOutErrorDescription) override;

 private:
  ~GMPServiceParent();

  RefPtr<GeckoMediaPluginServiceParent> mService;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPServiceParent_h_

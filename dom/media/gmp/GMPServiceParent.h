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
#include "nsDataHashtable.h"
#include "mozilla/Atomics.h"
#include "nsThreadUtils.h"

template <class> struct already_AddRefed;

namespace mozilla {
namespace gmp {

class GMPParent;

#define GMP_DEFAULT_ASYNC_SHUTDONW_TIMEOUT 3000

class GeckoMediaPluginServiceParent final : public GeckoMediaPluginService
                                          , public mozIGeckoMediaPluginChromeService
{
public:
  static already_AddRefed<GeckoMediaPluginServiceParent> GetSingleton();

  GeckoMediaPluginServiceParent();
  virtual nsresult Init() override;

  NS_DECL_ISUPPORTS_INHERITED

  // mozIGeckoMediaPluginService
  NS_IMETHOD GetPluginVersionForAPI(const nsACString& aAPI,
                                    nsTArray<nsCString>* aTags,
                                    bool* aHasPlugin,
                                    nsACString& aOutVersion) override;
  NS_IMETHOD GetNodeId(const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       bool aInPrivateBrowsingMode,
                       UniquePtr<GetNodeIdCallback>&& aCallback) override;

  NS_DECL_MOZIGECKOMEDIAPLUGINCHROMESERVICE
  NS_DECL_NSIOBSERVER

  void AsyncShutdownNeeded(GMPParent* aParent);
  void AsyncShutdownComplete(GMPParent* aParent);

  int32_t AsyncShutdownTimeoutMs();
#ifdef MOZ_CRASHREPORTER
  void SetAsyncShutdownPluginState(GMPParent* aGMPParent, char aId, const nsCString& aState);
#endif // MOZ_CRASHREPORTER

private:
  friend class GMPServiceParent;

  virtual ~GeckoMediaPluginServiceParent();

  void ClearStorage();

  GMPParent* SelectPluginForAPI(const nsACString& aNodeId,
                                const nsCString& aAPI,
                                const nsTArray<nsCString>& aTags);
  GMPParent* FindPluginForAPIFrom(size_t aSearchStartIndex,
                                  const nsCString& aAPI,
                                  const nsTArray<nsCString>& aTags,
                                  size_t* aOutPluginIndex);

  nsresult GetNodeId(const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
                     bool aInPrivateBrowsing, nsACString& aOutId);

  void UnloadPlugins();
  void CrashPlugins();
  void NotifySyncShutdownComplete();
  void NotifyAsyncShutdownComplete();

  void LoadFromEnvironment();
  void ProcessPossiblePlugin(nsIFile* aDir);

  void AddOnGMPThread(const nsAString& aDirectory);
  void RemoveOnGMPThread(const nsAString& aDirectory,
                         const bool aDeleteFromDisk,
                         const bool aCanDefer);

  nsresult SetAsyncShutdownTimeout();

  struct DirectoryFilter {
    virtual bool operator()(nsIFile* aPath) = 0;
    ~DirectoryFilter() {}
  };
  void ClearNodeIdAndPlugin(DirectoryFilter& aFilter);

  void ForgetThisSiteOnGMPThread(const nsACString& aOrigin);
  void ClearRecentHistoryOnGMPThread(PRTime aSince);

protected:
  friend class GMPParent;
  void ReAddOnGMPThread(const nsRefPtr<GMPParent>& aOld);
  void PluginTerminated(const nsRefPtr<GMPParent>& aOld);
  virtual void InitializePlugins() override;
  virtual bool GetContentParentFrom(const nsACString& aNodeId,
                                    const nsCString& aAPI,
                                    const nsTArray<nsCString>& aTags,
                                    UniquePtr<GetGMPContentParentCallback>&& aCallback)
    override;
private:
  GMPParent* ClonePlugin(const GMPParent* aOriginal);
  nsresult EnsurePluginsOnDiskScanned();
  nsresult InitStorage();

  class PathRunnable : public nsRunnable
  {
  public:
    enum EOperation {
      ADD,
      REMOVE,
      REMOVE_AND_DELETE_FROM_DISK,
    };

    PathRunnable(GeckoMediaPluginServiceParent* aService, const nsAString& aPath,
                 EOperation aOperation, bool aDefer = false)
      : mService(aService)
      , mPath(aPath)
      , mOperation(aOperation)
      , mDefer(aDefer)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsRefPtr<GeckoMediaPluginServiceParent> mService;
    nsString mPath;
    EOperation mOperation;
    bool mDefer;
  };

  // Protected by mMutex from the base class.
  nsTArray<nsRefPtr<GMPParent>> mPlugins;
  bool mShuttingDown;
  nsTArray<nsRefPtr<GMPParent>> mAsyncShutdownPlugins;
#ifdef MOZ_CRASHREPORTER
  class AsyncShutdownPluginStates
  {
  public:
    void Update(const nsCString& aPlugin, const nsCString& aInstance,
                char aId, const nsCString& aState);
  private:
    struct State { nsAutoCString mStateSequence; nsCString mLastStateDescription; };
    typedef nsClassHashtable<nsCStringHashKey, State> StatesByInstance;
    typedef nsClassHashtable<nsCStringHashKey, StatesByInstance> StateInstancesByPlugin;
    StateInstancesByPlugin mStates;
  } mAsyncShutdownPluginStates;
#endif // MOZ_CRASHREPORTER

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

  MainThreadOnly<bool> mWaitingForPluginsSyncShutdown;

  nsTArray<nsString> mPluginsWaitingForDeletion;

  nsCOMPtr<nsIFile> mStorageBaseDir;

  // Hashes of (origin,topLevelOrigin) to the node id for
  // non-persistent sessions.
  nsClassHashtable<nsUint32HashKey, nsCString> mTempNodeIds;

  // Hashes node id to whether that node id is allowed to store data
  // persistently on disk.
  nsDataHashtable<nsCStringHashKey, bool> mPersistentStorageAllowed;
};

nsresult ReadSalt(nsIFile* aPath, nsACString& aOutData);
bool MatchOrigin(nsIFile* aPath, const nsACString& aSite);

class GMPServiceParent final : public PGMPServiceParent
{
public:
  explicit GMPServiceParent(GeckoMediaPluginServiceParent* aService)
    : mService(aService)
  {
  }
  virtual ~GMPServiceParent();

  virtual bool RecvLoadGMP(const nsCString& aNodeId,
                           const nsCString& aApi,
                           nsTArray<nsCString>&& aTags,
                           nsTArray<ProcessId>&& aAlreadyBridgedTo,
                           base::ProcessId* aID,
                           nsCString* aDisplayName,
                           uint32_t* aPluginId) override;
  virtual bool RecvGetGMPNodeId(const nsString& aOrigin,
                                const nsString& aTopLevelOrigin,
                                const bool& aInPrivateBrowsing,
                                nsCString* aID) override;
  static bool RecvGetGMPPluginVersionForAPI(const nsCString& aAPI,
                                            nsTArray<nsCString>&& aTags,
                                            bool* aHasPlugin,
                                            nsCString* aVersion);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  static PGMPServiceParent* Create(Transport* aTransport, ProcessId aOtherPid);

private:
  nsRefPtr<GeckoMediaPluginServiceParent> mService;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPServiceParent_h_

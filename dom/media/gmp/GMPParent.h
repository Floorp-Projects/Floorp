/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPParent_h_
#define GMPParent_h_

#include "GMPProcessParent.h"
#include "GMPServiceParent.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"
#include "GMPTimerParent.h"
#include "GMPStorageParent.h"
#include "mozilla/gmp/PGMPParent.h"
#include "mozilla/ipc/CrashReporterHelper.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"
#include "mozilla/MozPromise.h"

namespace mozilla::gmp {

class GMPCapability {
 public:
  explicit GMPCapability() = default;
  GMPCapability(GMPCapability&& aOther)
      : mAPIName(std::move(aOther.mAPIName)),
        mAPITags(std::move(aOther.mAPITags)) {}
  explicit GMPCapability(const nsACString& aAPIName) : mAPIName(aAPIName) {}
  explicit GMPCapability(const GMPCapability& aOther) = default;
  nsCString mAPIName;
  CopyableTArray<nsCString> mAPITags;

  static bool Supports(const nsTArray<GMPCapability>& aCapabilities,
                       const nsACString& aAPI,
                       const nsTArray<nsCString>& aTags);

  static bool Supports(const nsTArray<GMPCapability>& aCapabilities,
                       const nsACString& aAPI, const nsCString& aTag);
};

enum GMPState {
  GMPStateNotLoaded,
  GMPStateLoaded,
  GMPStateUnloading,
  GMPStateClosing
};

class GMPContentParent;

class GMPParent final
    : public PGMPParent,
      public ipc::CrashReporterHelper<GeckoProcessType_GMPlugin> {
  friend class PGMPParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPParent)

  GMPParent();

  RefPtr<GenericPromise> Init(GeckoMediaPluginServiceParent* aService,
                              nsIFile* aPluginDir);
  void CloneFrom(const GMPParent* aOther);

  void Crash();

  nsresult LoadProcess();

  // Called internally to close this if we don't need it
  void CloseIfUnused();

  // Notify all active de/encoders that we are closing, either because of
  // normal shutdown or unexpected shutdown/crash.
  void CloseActive(bool aDieWhenUnloaded);

  // Tell the plugin to die after shutdown.
  void MarkForDeletion();
  bool IsMarkedForDeletion();

  // Called by the GMPService to forcibly close active de/encoders at shutdown
  void Shutdown();

  // This must not be called while we're in the middle of abnormal ActorDestroy
  void DeleteProcess();

  GMPState State() const;
  nsCOMPtr<nsISerialEventTarget> GMPEventTarget();

  // A GMP can either be a single instance shared across all NodeIds (like
  // in the OpenH264 case), or we can require a new plugin instance for every
  // NodeIds running the plugin (as in the EME plugin case).
  //
  // A NodeId is a hash of the ($urlBarOrigin, $ownerDocOrigin) pair.
  //
  // Plugins are associated with an NodeIds by calling SetNodeId() before
  // loading.
  //
  // If a plugin has no NodeId specified and it is loaded, it is assumed to
  // be shared across NodeIds.

  // Specifies that a GMP can only work with the specified NodeIds.
  void SetNodeId(const nsACString& aNodeId);
  const nsACString& GetNodeId() const { return mNodeId; }

  const nsCString& GetDisplayName() const;
  const nsCString& GetVersion() const;
  uint32_t GetPluginId() const;
  nsString GetPluginBaseName() const;

  // Returns true if a plugin can be or is being used across multiple NodeIds.
  bool CanBeSharedCrossNodeIds() const;

  // A GMP can be used from a NodeId if it's already been set to work with
  // that NodeId, or if it's not been set to work with any NodeId and has
  // not yet been loaded (i.e. it's not shared across NodeIds).
  bool CanBeUsedFrom(const nsACString& aNodeId) const;

  already_AddRefed<nsIFile> GetDirectory() {
    return nsCOMPtr<nsIFile>(mDirectory).forget();
  }

  void AbortAsyncShutdown();

  // Called when the child process has died.
  void ChildTerminated();

  bool OpenPGMPContent();

  void GetGMPContentParent(
      UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>>&& aPromiseHolder);
  already_AddRefed<GMPContentParent> ForgetGMPContentParent();

  bool EnsureProcessLoaded(base::ProcessId* aID);

  void IncrementGMPContentChildCount();

  const nsTArray<GMPCapability>& GetCapabilities() const {
    return mCapabilities;
  }

 private:
  ~GMPParent();

  RefPtr<GeckoMediaPluginServiceParent> mService;
  bool EnsureProcessLoaded();
  RefPtr<GenericPromise> ReadGMPMetaData();
  RefPtr<GenericPromise> ReadGMPInfoFile(nsIFile* aFile);
  RefPtr<GenericPromise> ParseChromiumManifest(
      const nsAString& aJSON);  // Worker thread.
  RefPtr<GenericPromise> ReadChromiumManifestFile(
      nsIFile* aFile);  // GMP thread.
  void AddCrashAnnotations();
  void GetCrashID(nsString& aResult);
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPGMPStorageConstructor(
      PGMPStorageParent* actor) override;
  PGMPStorageParent* AllocPGMPStorageParent();
  bool DeallocPGMPStorageParent(PGMPStorageParent* aActor);

  mozilla::ipc::IPCResult RecvPGMPTimerConstructor(
      PGMPTimerParent* actor) override;
  PGMPTimerParent* AllocPGMPTimerParent();
  bool DeallocPGMPTimerParent(PGMPTimerParent* aActor);

  mozilla::ipc::IPCResult RecvPGMPContentChildDestroyed();

  mozilla::ipc::IPCResult RecvFOGData(ByteBuf&& aBuf);

  bool IsUsed() {
    return mGMPContentChildCount > 0 || !mGetContentParentPromises.IsEmpty();
  }

  void ResolveGetContentParentPromises();
  void RejectGetContentParentPromises();

#if defined(XP_MACOSX) && defined(__aarch64__)
  // We pre-translate XUL and our plugin file to avoid x64 child process
  // startup delays caused by translation for instances when the child
  // process binary translations have not already been cached. i.e., the
  // first time we launch an x64 child process after installation or
  // update. Measured by binary size of a recent XUL and Widevine plugin,
  // this makes up 94% of the translation needed. Re-translating the
  // same binary does not cause translation to occur again.
  void PreTranslateBins();
  void PreTranslateBinsWorker();
#endif

#if defined(XP_MACOSX)
  nsresult GetPluginFileArch(nsIFile* aPluginDir, nsAutoString& aLeafName,
                             uint32_t& aArchSet);
#endif

  GMPState mState;
  nsCOMPtr<nsIFile> mDirectory;  // plugin directory on disk
  nsString mName;  // base name of plugin on disk, UTF-16 because used for paths
  nsCString mDisplayName;  // name of plugin displayed to users
  nsCString mDescription;  // description of plugin for display to users
  nsCString mVersion;
#if defined(XP_WIN) || defined(XP_LINUX)
  nsCString mLibs;
#endif
  nsString mAdapter;
  const uint32_t mPluginId;
  nsTArray<GMPCapability> mCapabilities;
  GMPProcessParent* mProcess;
  bool mDeleteProcessOnlyOnUnload;
  bool mAbnormalShutdownInProgress;
  bool mIsBlockingDeletion;

  bool mCanDecrypt;

  nsTArray<RefPtr<GMPTimerParent>> mTimers;
  nsTArray<RefPtr<GMPStorageParent>> mStorage;
  // NodeId the plugin is assigned to, or empty if the the plugin is not
  // assigned to a NodeId.
  nsCString mNodeId;
  // This is used for GMP content in the parent, there may be more of these in
  // the content processes.
  RefPtr<GMPContentParent> mGMPContentParent;
  nsTArray<UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>>>
      mGetContentParentPromises;
  uint32_t mGMPContentChildCount;

  int mChildPid;

  // We hold a self reference to ourself while the child process is alive.
  // This ensures that if the GMPService tries to shut us down and drops
  // its reference to us, we stay alive long enough for the child process
  // to terminate gracefully.
  bool mHoldingSelfRef;

#if defined(XP_MACOSX) && defined(__aarch64__)
  // The child process architecture to use.
  uint32_t mChildLaunchArch;
  nsCString mPluginFilePath;
#endif

  const nsCOMPtr<nsISerialEventTarget> mMainThread;
};

}  // namespace mozilla::gmp

#endif  // GMPParent_h_

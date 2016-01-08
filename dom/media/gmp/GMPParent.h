/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPParent_h_
#define GMPParent_h_

#include "GMPProcessParent.h"
#include "GMPServiceParent.h"
#include "GMPAudioDecoderParent.h"
#include "GMPDecryptorParent.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"
#include "GMPTimerParent.h"
#include "GMPStorageParent.h"
#include "mozilla/gmp/PGMPParent.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"

class nsIThread;

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"

namespace mozilla {
namespace dom {
class PCrashReporterParent;
class CrashReporterParent;
}
}
#endif

namespace mozilla {
namespace gmp {

class GMPCapability
{
public:
  nsCString mAPIName;
  nsTArray<nsCString> mAPITags;
};

enum GMPState {
  GMPStateNotLoaded,
  GMPStateLoaded,
  GMPStateUnloading,
  GMPStateClosing
};

class GMPContentParent;

class GetGMPContentParentCallback
{
public:
  GetGMPContentParentCallback()
  {
    MOZ_COUNT_CTOR(GetGMPContentParentCallback);
  };
  virtual ~GetGMPContentParentCallback()
  {
    MOZ_COUNT_DTOR(GetGMPContentParentCallback);
  };
  virtual void Done(GMPContentParent* aGMPContentParent) = 0;
};

class GMPParent final : public PGMPParent
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPParent)

  GMPParent();

  nsresult Init(GeckoMediaPluginServiceParent* aService, nsIFile* aPluginDir);
  nsresult CloneFrom(const GMPParent* aOther);

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

  bool SupportsAPI(const nsCString& aAPI, const nsCString& aTag);

  GMPState State() const;
  nsIThread* GMPThread();

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

  bool GetGMPContentParent(UniquePtr<GetGMPContentParentCallback>&& aCallback);
  already_AddRefed<GMPContentParent> ForgetGMPContentParent();

  bool EnsureProcessLoaded(base::ProcessId* aID);

  bool Bridge(GMPServiceParent* aGMPServiceParent);

private:
  ~GMPParent();
  RefPtr<GeckoMediaPluginServiceParent> mService;
  bool EnsureProcessLoaded();
  nsresult ReadGMPMetaData();
#ifdef MOZ_CRASHREPORTER
  void WriteExtraDataForMinidump(CrashReporter::AnnotationTable& notes);
  void GetCrashID(nsString& aResult);
#endif
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PCrashReporterParent* AllocPCrashReporterParent(const NativeThreadId& aThread) override;
  virtual bool DeallocPCrashReporterParent(PCrashReporterParent* aCrashReporter) override;

  virtual bool RecvPGMPStorageConstructor(PGMPStorageParent* actor) override;
  virtual PGMPStorageParent* AllocPGMPStorageParent() override;
  virtual bool DeallocPGMPStorageParent(PGMPStorageParent* aActor) override;

  virtual PGMPContentParent* AllocPGMPContentParent(Transport* aTransport,
                                                    ProcessId aOtherPid) override;

  virtual bool RecvPGMPTimerConstructor(PGMPTimerParent* actor) override;
  virtual PGMPTimerParent* AllocPGMPTimerParent() override;
  virtual bool DeallocPGMPTimerParent(PGMPTimerParent* aActor) override;

  virtual bool RecvAsyncShutdownComplete() override;
  virtual bool RecvAsyncShutdownRequired() override;

  virtual bool RecvPGMPContentChildDestroyed() override;
  bool IsUsed()
  {
    return mGMPContentChildCount > 0;
  }


  static void AbortWaitingForGMPAsyncShutdown(nsITimer* aTimer, void* aClosure);
  nsresult EnsureAsyncShutdownTimeoutSet();

  GMPState mState;
  nsCOMPtr<nsIFile> mDirectory; // plugin directory on disk
  nsString mName; // base name of plugin on disk, UTF-16 because used for paths
  nsCString mDisplayName; // name of plugin displayed to users
  nsCString mDescription; // description of plugin for display to users
  nsCString mVersion;
  uint32_t mPluginId;
  nsTArray<nsAutoPtr<GMPCapability>> mCapabilities;
  GMPProcessParent* mProcess;
  bool mDeleteProcessOnlyOnUnload;
  bool mAbnormalShutdownInProgress;
  bool mIsBlockingDeletion;

  bool mCanDecrypt;

  nsTArray<RefPtr<GMPTimerParent>> mTimers;
  nsTArray<RefPtr<GMPStorageParent>> mStorage;
  nsCOMPtr<nsIThread> mGMPThread;
  nsCOMPtr<nsITimer> mAsyncShutdownTimeout; // GMP Thread only.
  // NodeId the plugin is assigned to, or empty if the the plugin is not
  // assigned to a NodeId.
  nsCString mNodeId;
  // This is used for GMP content in the parent, there may be more of these in
  // the content processes.
  RefPtr<GMPContentParent> mGMPContentParent;
  nsTArray<UniquePtr<GetGMPContentParentCallback>> mCallbacks;
  uint32_t mGMPContentChildCount;

  bool mAsyncShutdownRequired;
  bool mAsyncShutdownInProgress;

  int mChildPid;

  // We hold a self reference to ourself while the child process is alive.
  // This ensures that if the GMPService tries to shut us down and drops
  // its reference to us, we stay alive long enough for the child process
  // to terminate gracefully.
  bool mHoldingSelfRef;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_

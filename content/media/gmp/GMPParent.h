/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPParent_h_
#define GMPParent_h_

#include "GMPProcessParent.h"
#include "GMPService.h"
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
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class nsILineInputStream;
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

class GMPParent MOZ_FINAL : public PGMPParent,
                            public GMPSharedMem
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(GMPParent)

  GMPParent();

  nsresult Init(GeckoMediaPluginService *aService, nsIFile* aPluginDir);
  nsresult CloneFrom(const GMPParent* aOther);

  void Crash();

  nsresult LoadProcess();

  // Called internally to close this if we don't need it
  void CloseIfUnused();

  // Notify all active de/encoders that we are closing, either because of
  // normal shutdown or unexpected shutdown/crash.
  void CloseActive(bool aDieWhenUnloaded);

  // Called by the GMPService to forcibly close active de/encoders at shutdown
  void Shutdown();

  // This must not be called while we're in the middle of abnormal ActorDestroy
  void DeleteProcess();

  bool SupportsAPI(const nsCString& aAPI, const nsCString& aTag);

  nsresult GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD);
  void VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder);

  nsresult GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE);
  void VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder);

  nsresult GetGMPDecryptor(GMPDecryptorParent** aGMPKS);
  void DecryptorDestroyed(GMPDecryptorParent* aSession);

  nsresult GetGMPAudioDecoder(GMPAudioDecoderParent** aGMPAD);
  void AudioDecoderDestroyed(GMPAudioDecoderParent* aDecoder);

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

  // Returns true if a plugin can be or is being used across multiple NodeIds.
  bool CanBeSharedCrossNodeIds() const;

  // A GMP can be used from a NodeId if it's already been set to work with
  // that NodeId, or if it's not been set to work with any NodeId and has
  // not yet been loaded (i.e. it's not shared across NodeIds).
  bool CanBeUsedFrom(const nsACString& aNodeId) const;

  already_AddRefed<nsIFile> GetDirectory() {
    return nsCOMPtr<nsIFile>(mDirectory).forget();
  }

  // GMPSharedMem
  virtual void CheckThread() MOZ_OVERRIDE;

  void AbortAsyncShutdown();

  bool HasAccessedStorage() const;

private:
  ~GMPParent();
  nsRefPtr<GeckoMediaPluginService> mService;
  bool EnsureProcessLoaded();
  nsresult ReadGMPMetaData();
#ifdef MOZ_CRASHREPORTER
  void WriteExtraDataForMinidump(CrashReporter::AnnotationTable& notes);
  void GetCrashID(nsString& aResult);
#endif
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PCrashReporterParent* AllocPCrashReporterParent(const NativeThreadId& aThread) MOZ_OVERRIDE;
  virtual bool DeallocPCrashReporterParent(PCrashReporterParent* aCrashReporter) MOZ_OVERRIDE;

  virtual PGMPVideoDecoderParent* AllocPGMPVideoDecoderParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor) MOZ_OVERRIDE;

  virtual PGMPVideoEncoderParent* AllocPGMPVideoEncoderParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor) MOZ_OVERRIDE;

  virtual PGMPDecryptorParent* AllocPGMPDecryptorParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPDecryptorParent(PGMPDecryptorParent* aActor) MOZ_OVERRIDE;

  virtual PGMPAudioDecoderParent* AllocPGMPAudioDecoderParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPAudioDecoderParent(PGMPAudioDecoderParent* aActor) MOZ_OVERRIDE;

  virtual bool RecvPGMPStorageConstructor(PGMPStorageParent* actor) MOZ_OVERRIDE;
  virtual PGMPStorageParent* AllocPGMPStorageParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPStorageParent(PGMPStorageParent* aActor) MOZ_OVERRIDE;

  virtual bool RecvPGMPTimerConstructor(PGMPTimerParent* actor) MOZ_OVERRIDE;
  virtual PGMPTimerParent* AllocPGMPTimerParent() MOZ_OVERRIDE;
  virtual bool DeallocPGMPTimerParent(PGMPTimerParent* aActor) MOZ_OVERRIDE;

  virtual bool RecvAsyncShutdownComplete() MOZ_OVERRIDE;
  virtual bool RecvAsyncShutdownRequired() MOZ_OVERRIDE;

  nsresult EnsureAsyncShutdownTimeoutSet();

  GMPState mState;
  nsCOMPtr<nsIFile> mDirectory; // plugin directory on disk
  nsString mName; // base name of plugin on disk, UTF-16 because used for paths
  nsCString mDisplayName; // name of plugin displayed to users
  nsCString mDescription; // description of plugin for display to users
  nsCString mVersion;
  nsTArray<nsAutoPtr<GMPCapability>> mCapabilities;
  GMPProcessParent* mProcess;
  bool mDeleteProcessOnlyOnUnload;
  bool mAbnormalShutdownInProgress;

  nsTArray<nsRefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<nsRefPtr<GMPVideoEncoderParent>> mVideoEncoders;
  nsTArray<nsRefPtr<GMPDecryptorParent>> mDecryptors;
  nsTArray<nsRefPtr<GMPAudioDecoderParent>> mAudioDecoders;
  nsTArray<nsRefPtr<GMPTimerParent>> mTimers;
  nsTArray<nsRefPtr<GMPStorageParent>> mStorage;
  nsCOMPtr<nsIThread> mGMPThread;
  nsCOMPtr<nsITimer> mAsyncShutdownTimeout; // GMP Thread only.
  // NodeId the plugin is assigned to, or empty if the the plugin is not
  // assigned to a NodeId.
  nsAutoCString mNodeId;

  bool mAsyncShutdownRequired;
  bool mAsyncShutdownInProgress;
  bool mHasAccessedStorage;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_

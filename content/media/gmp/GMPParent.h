/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPParent_h_
#define GMPParent_h_

#include "GMPProcessParent.h"
#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncoderParent.h"
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

class GMPParent MOZ_FINAL : public PGMPParent
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(GMPParent)

  GMPParent();

  nsresult Init(nsIFile* aPluginDir);
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
  GMPState State() const;
#ifdef DEBUG
  nsIThread* GMPThread();
#endif

  // A GMP can either be a single instance shared across all origins (like
  // in the OpenH264 case), or we can require a new plugin instance for every
  // origin running the plugin (as in the EME plugin case).
  //
  // Plugins are associated with an origin by calling SetOrigin() before
  // loading.
  //
  // If a plugin has no origin specified and it is loaded, it is assumed to
  // be shared across origins.

  // Specifies that a GMP can only work with the specified origin.
  void SetOrigin(const nsAString& aOrigin);

  // Returns true if a plugin can be or is being used across multiple origins.
  bool CanBeSharedCrossOrigin() const;

  // A GMP can be used from an origin if it's already been set to work with
  // that origin, or if it's not been set to work with any origin and has
  // not yet been loaded (i.e. it's not shared across origins).
  bool CanBeUsedFrom(const nsAString& aOrigin) const;

  already_AddRefed<nsIFile> GetDirectory() {
    return nsCOMPtr<nsIFile>(mDirectory).forget();
  }

private:
  ~GMPParent();
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

  GMPState mState;
  nsCOMPtr<nsIFile> mDirectory; // plugin directory on disk
  nsString mName; // base name of plugin on disk, UTF-16 because used for paths
  nsCString mDisplayName; // name of plugin displayed to users
  nsCString mDescription; // description of plugin for display to users
  nsCString mVersion;
  nsTArray<nsAutoPtr<GMPCapability>> mCapabilities;
  GMPProcessParent* mProcess;
  bool mDeleteProcessOnUnload;

  nsTArray<nsRefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<nsRefPtr<GMPVideoEncoderParent>> mVideoEncoders;
#ifdef DEBUG
  nsCOMPtr<nsIThread> mGMPThread;
#endif
  // Origin the plugin is assigned to, or empty if the the plugin is not
  // assigned to an origin.
  nsAutoString mOrigin;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_

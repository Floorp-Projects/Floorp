/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPParent.h"
#include "prlog.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/unused.h"
#include "nsIObserverService.h"
#include "GMPTimerParent.h"
#include "runnable_utils.h"
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/Sandbox.h"
#endif

#include "mozilla/dom/CrashReporterParent.h"
using mozilla::dom::CrashReporterParent;

#ifdef MOZ_CRASHREPORTER
using CrashReporter::AnnotationTable;
using CrashReporter::GetIDFromMinidump;
#endif

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetGMPLog();

#define LOGD(msg) PR_LOG(GetGMPLog(), PR_LOG_DEBUG, msg)
#define LOG(level, msg) PR_LOG(GetGMPLog(), (level), msg)
#else
#define LOGD(msg)
#define LOG(level, msg)
#endif

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPParent"

namespace gmp {

GMPParent::GMPParent()
  : mState(GMPStateNotLoaded)
  , mProcess(nullptr)
  , mDeleteProcessOnlyOnUnload(false)
  , mAbnormalShutdownInProgress(false)
  , mAsyncShutdownRequired(false)
  , mAsyncShutdownInProgress(false)
  , mHasAccessedStorage(false)
{
}

GMPParent::~GMPParent()
{
  // Can't Close or Destroy the process here, since destruction is MainThread only
  MOZ_ASSERT(NS_IsMainThread());
}

void
GMPParent::CheckThread()
{
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());
}

nsresult
GMPParent::CloneFrom(const GMPParent* aOther)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  MOZ_ASSERT(aOther->mDirectory && aOther->mService, "null plugin directory");
  return Init(aOther->mService, aOther->mDirectory);
}

nsresult
GMPParent::Init(GeckoMediaPluginService *aService, nsIFile* aPluginDir)
{
  MOZ_ASSERT(aPluginDir);
  MOZ_ASSERT(aService);
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  mService = aService;
  mDirectory = aPluginDir;

  // aPluginDir is <profile-dir>/<gmp-plugin-id>/<version>
  // where <gmp-plugin-id> should be gmp-gmpopenh264
  nsCOMPtr<nsIFile> parent;
  nsresult rv = aPluginDir->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_FAILED(rv)) {
    return rv;
  }
  LOGD(("%s::%s: %p for %s", __CLASS__, __FUNCTION__, this,
       NS_LossyConvertUTF16toASCII(parentLeafName).get()));

  MOZ_ASSERT(parentLeafName.Length() > 4);
  mName = Substring(parentLeafName, 4);

  return ReadGMPMetaData();
}

void
GMPParent::Crash()
{
  if (mState != GMPStateNotLoaded) {
    unused << SendCrashPluginNow();
  }
}

nsresult
GMPParent::LoadProcess()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  MOZ_ASSERT(mState == GMPStateNotLoaded);

  nsAutoString path;
  if (NS_FAILED(mDirectory->GetPath(path))) {
    return NS_ERROR_FAILURE;
  }
  LOGD(("%s::%s: %p for %s", __CLASS__, __FUNCTION__, this, path.get()));

  if (!mProcess) {
    mProcess = new GMPProcessParent(NS_ConvertUTF16toUTF8(path).get());
    if (!mProcess->Launch(30 * 1000)) {
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }

    bool opened = Open(mProcess->GetChannel(), mProcess->GetChildProcessHandle());
    if (!opened) {
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }
    LOGD(("%s::%s: Created new process %p", __CLASS__, __FUNCTION__, (void *)mProcess));

    bool ok = SendSetNodeId(mNodeId);
    if (!ok) {
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }
    LOGD(("%s::%s: Failed to send node id %p", __CLASS__, __FUNCTION__, (void *)mProcess));

    ok = SendStartPlugin();
    if (!ok) {
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }
    LOGD(("%s::%s: Failed to send start %p", __CLASS__, __FUNCTION__, (void *)mProcess));
  }

  mState = GMPStateLoaded;

  return NS_OK;
}

void
AbortWaitingForGMPAsyncShutdown(nsITimer* aTimer, void* aClosure)
{
  NS_WARNING("Timed out waiting for GMP async shutdown!");
  GMPParent* parent = reinterpret_cast<GMPParent*>(aClosure);
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  if (service) {
    service->AsyncShutdownComplete(parent);
  }
}

nsresult
GMPParent::EnsureAsyncShutdownTimeoutSet()
{
  if (mAsyncShutdownTimeout) {
    return NS_OK;
  }

  nsresult rv;
  mAsyncShutdownTimeout = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set timer to abort waiting for plugin to shutdown if it takes
  // too long.
  rv = mAsyncShutdownTimeout->SetTarget(mGMPThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
   return rv;
  }

  int32_t timeout = GMP_DEFAULT_ASYNC_SHUTDONW_TIMEOUT;
  nsRefPtr<GeckoMediaPluginService> service =
    GeckoMediaPluginService::GetGeckoMediaPluginService();
  if (service) {
    timeout = service->AsyncShutdownTimeoutMs();
  }
  return mAsyncShutdownTimeout->InitWithFuncCallback(
    &AbortWaitingForGMPAsyncShutdown, this, timeout,
    nsITimer::TYPE_ONE_SHOT);
}

void
GMPParent::CloseIfUnused()
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  LOGD(("%s::%s: %p mAsyncShutdownRequired=%d", __CLASS__, __FUNCTION__, this,
        mAsyncShutdownRequired));

  if ((mDeleteProcessOnlyOnUnload ||
       mState == GMPStateLoaded ||
       mState == GMPStateUnloading) &&
      mVideoDecoders.IsEmpty() &&
      mVideoEncoders.IsEmpty() &&
      mDecryptors.IsEmpty() &&
      mAudioDecoders.IsEmpty()) {

    // Ensure all timers are killed.
    for (uint32_t i = mTimers.Length(); i > 0; i--) {
      mTimers[i - 1]->Shutdown();
    }

    if (mAsyncShutdownRequired) {
      if (!mAsyncShutdownInProgress) {
        LOGD(("%s::%s: %p sending async shutdown notification", __CLASS__,
              __FUNCTION__, this));
        mAsyncShutdownInProgress = true;
        if (!SendBeginAsyncShutdown() ||
            NS_FAILED(EnsureAsyncShutdownTimeoutSet())) {
          AbortAsyncShutdown();
        }
      }
    } else {
      // Any async shutdown must be complete. Shutdown GMPStorage.
      for (size_t i = mStorage.Length(); i > 0; i--) {
        mStorage[i - 1]->Shutdown();
      }
      Shutdown();
    }
  }
}

void
GMPParent::AbortAsyncShutdown()
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));

  if (mAsyncShutdownTimeout) {
    mAsyncShutdownTimeout->Cancel();
    mAsyncShutdownTimeout = nullptr;
  }

  if (!mAsyncShutdownRequired || !mAsyncShutdownInProgress) {
    return;
  }

  nsRefPtr<GMPParent> kungFuDeathGrip(this);
  mService->AsyncShutdownComplete(this);
  mAsyncShutdownRequired = false;
  mAsyncShutdownInProgress = false;
  CloseIfUnused();
}

void
GMPParent::AudioDecoderDestroyed(GMPAudioDecoderParent* aDecoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mAudioDecoders.RemoveElement(aDecoder));

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::CloseIfUnused);
  NS_DispatchToCurrentThread(event);
}

void
GMPParent::CloseActive(bool aDieWhenUnloaded)
{
  LOGD(("%s::%s: %p state %d", __CLASS__, __FUNCTION__, this, mState));
  if (aDieWhenUnloaded) {
    mDeleteProcessOnlyOnUnload = true; // don't allow this to go back...
  }
  if (mState == GMPStateLoaded) {
    mState = GMPStateUnloading;
  }

  // Invalidate and remove any remaining API objects.
  for (uint32_t i = mVideoDecoders.Length(); i > 0; i--) {
    mVideoDecoders[i - 1]->Shutdown();
  }

  for (uint32_t i = mVideoEncoders.Length(); i > 0; i--) {
    mVideoEncoders[i - 1]->Shutdown();
  }

  for (uint32_t i = mDecryptors.Length(); i > 0; i--) {
    mDecryptors[i - 1]->Shutdown();
  }

  for (uint32_t i = mAudioDecoders.Length(); i > 0; i--) {
    mAudioDecoders[i - 1]->Shutdown();
  }

  // Note: we don't shutdown timers here, we do that in CloseIfUnused(),
  // as there are multiple entry points to CloseIfUnused().

  // Note: We don't shutdown storage API objects here, as they need to
  // work during async shutdown of GMPs.

  // Note: the shutdown of the codecs is async!  don't kill
  // the plugin-container until they're all safely shut down via
  // CloseIfUnused();
  CloseIfUnused();
}

void
GMPParent::Shutdown()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ASSERT(!mAsyncShutdownTimeout, "Should have canceled shutdown timeout");

  if (mAbnormalShutdownInProgress) {
    return;
  }
  MOZ_ASSERT(mVideoDecoders.IsEmpty() && mVideoEncoders.IsEmpty());
  if (mState == GMPStateNotLoaded || mState == GMPStateClosing) {
    return;
  }

  DeleteProcess();
  // XXX Get rid of mDeleteProcessOnlyOnUnload and this code when
  // Bug 1043671 is fixed
  if (!mDeleteProcessOnlyOnUnload) {
    // Destroy ourselves and rise from the fire to save memory
    nsRefPtr<GMPParent> self(this);
    mService->ReAddOnGMPThread(self);
  } // else we've been asked to die and stay dead
  MOZ_ASSERT(mState == GMPStateNotLoaded);
}

void
GMPParent::DeleteProcess()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));

  if (mState != GMPStateClosing) {
    // Don't Close() twice!
    // Probably remove when bug 1043671 is resolved
    mState = GMPStateClosing;
    Close();
  }
  mProcess->Delete();
  LOGD(("%s::%s: Shut down process %p", __CLASS__, __FUNCTION__, (void *) mProcess));
  mProcess = nullptr;
  mState = GMPStateNotLoaded;
}

void
GMPParent::VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  // If the constructor fails, we'll get called before it's added
  unused << NS_WARN_IF(!mVideoDecoders.RemoveElement(aDecoder));

  if (mVideoDecoders.IsEmpty() &&
      mVideoEncoders.IsEmpty()) {
    // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
    // until after this has completed.
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::CloseIfUnused);
    NS_DispatchToCurrentThread(event);
  }
}

void
GMPParent::VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  // If the constructor fails, we'll get called before it's added
  unused << NS_WARN_IF(!mVideoEncoders.RemoveElement(aEncoder));

  if (mVideoDecoders.IsEmpty() &&
      mVideoEncoders.IsEmpty()) {
    // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
    // until after this has completed.
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::CloseIfUnused);
    NS_DispatchToCurrentThread(event);
  }
}

void
GMPParent::DecryptorDestroyed(GMPDecryptorParent* aSession)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mDecryptors.RemoveElement(aSession));

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  if (mDecryptors.IsEmpty()) {
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::CloseIfUnused);
    NS_DispatchToCurrentThread(event);
  }
}

nsresult
GMPParent::GetGMPDecryptor(GMPDecryptorParent** aGMPDP)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPDecryptorParent* pdp = SendPGMPDecryptorConstructor();
  if (!pdp) {
    return NS_ERROR_FAILURE;
  }
  GMPDecryptorParent* dp = static_cast<GMPDecryptorParent*>(pdp);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(dp);
  mDecryptors.AppendElement(dp);
  *aGMPDP = dp;

  return NS_OK;
}

GMPState
GMPParent::State() const
{
  return mState;
}

// Not changing to use mService since we'll be removing it
nsIThread*
GMPParent::GMPThread()
{
  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    MOZ_ASSERT(mps);
    if (!mps) {
      return nullptr;
    }
    // Not really safe if we just grab to the mGMPThread, as we don't know
    // what thread we're running on and other threads may be trying to
    // access this without locks!  However, debug only, and primary failure
    // mode outside of compiler-helped TSAN is a leak.  But better would be
    // to use swap() under a lock.
    mps->GetThread(getter_AddRefs(mGMPThread));
    MOZ_ASSERT(mGMPThread);
  }

  return mGMPThread;
}

bool
GMPParent::SupportsAPI(const nsCString& aAPI, const nsCString& aTag)
{
  for (uint32_t i = 0; i < mCapabilities.Length(); i++) {
    if (!mCapabilities[i]->mAPIName.Equals(aAPI)) {
      continue;
    }
    nsTArray<nsCString>& tags = mCapabilities[i]->mAPITags;
    for (uint32_t j = 0; j < tags.Length(); j++) {
      if (tags[j].Equals(aTag)) {
        return true;
      }
    }
  }
  return false;
}

bool
GMPParent::EnsureProcessLoaded()
{
  if (mState == GMPStateLoaded) {
    return true;
  }
  if (mState == GMPStateClosing ||
      mState == GMPStateUnloading) {
    return false;
  }

  nsresult rv = LoadProcess();

  return NS_SUCCEEDED(rv);
}

nsresult
GMPParent::GetGMPAudioDecoder(GMPAudioDecoderParent** aGMPAD)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPAudioDecoderParent* pvap = SendPGMPAudioDecoderConstructor();
  if (!pvap) {
    return NS_ERROR_FAILURE;
  }
  GMPAudioDecoderParent* vap = static_cast<GMPAudioDecoderParent*>(pvap);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vap);
  *aGMPAD = vap;
  mAudioDecoders.AppendElement(vap);

  return NS_OK;
}

nsresult
GMPParent::GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  // returned with one anonymous AddRef that locks it until Destroy
  PGMPVideoDecoderParent* pvdp = SendPGMPVideoDecoderConstructor();
  if (!pvdp) {
    return NS_ERROR_FAILURE;
  }
  GMPVideoDecoderParent *vdp = static_cast<GMPVideoDecoderParent*>(pvdp);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vdp);
  *aGMPVD = vdp;
  mVideoDecoders.AppendElement(vdp);

  return NS_OK;
}

nsresult
GMPParent::GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  // returned with one anonymous AddRef that locks it until Destroy
  PGMPVideoEncoderParent* pvep = SendPGMPVideoEncoderConstructor();
  if (!pvep) {
    return NS_ERROR_FAILURE;
  }
  GMPVideoEncoderParent *vep = static_cast<GMPVideoEncoderParent*>(pvep);
  // This addref corresponds to the Proxy pointer the consumer is returned.
  // It's dropped by calling Close() on the interface.
  NS_ADDREF(vep);
  *aGMPVE = vep;
  mVideoEncoders.AppendElement(vep);

  return NS_OK;
}

#ifdef MOZ_CRASHREPORTER
void
GMPParent::WriteExtraDataForMinidump(CrashReporter::AnnotationTable& notes)
{
  notes.Put(NS_LITERAL_CSTRING("GMPPlugin"), NS_LITERAL_CSTRING("1"));
  notes.Put(NS_LITERAL_CSTRING("PluginFilename"),
                               NS_ConvertUTF16toUTF8(mName));
  notes.Put(NS_LITERAL_CSTRING("PluginName"), mDisplayName);
  notes.Put(NS_LITERAL_CSTRING("PluginVersion"), mVersion);
}

void
GMPParent::GetCrashID(nsString& aResult)
{
  CrashReporterParent* cr = nullptr;
  if (ManagedPCrashReporterParent().Length() > 0) {
    cr = static_cast<CrashReporterParent*>(ManagedPCrashReporterParent()[0]);
  }
  if (NS_WARN_IF(!cr)) {
    return;
  }

  AnnotationTable notes(4);
  WriteExtraDataForMinidump(notes);
  nsCOMPtr<nsIFile> dumpFile;
  TakeMinidump(getter_AddRefs(dumpFile), nullptr);
  if (!dumpFile) {
    NS_WARNING("GMP crash without crash report");
    return;
  }
  GetIDFromMinidump(dumpFile, aResult);
  cr->GenerateCrashReportForMinidump(dumpFile, &notes);
}

static void
GMPNotifyObservers(nsAString& aData)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsString temp(aData);
    obs->NotifyObservers(nullptr, "gmp-plugin-crash", temp.get());
  }
}
#endif
void
GMPParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("%s::%s: %p (%d)", __CLASS__, __FUNCTION__, this, (int) aWhy));
#ifdef MOZ_CRASHREPORTER
  if (AbnormalShutdown == aWhy) {
    nsString dumpID;
    GetCrashID(dumpID);
    nsString id;
    // use the parent address to identify it
    // We could use any unique-to-the-parent value
    id.AppendInt(reinterpret_cast<uint64_t>(this));
    id.Append(NS_LITERAL_STRING(" "));
    AppendUTF8toUTF16(mDisplayName, id);
    id.Append(NS_LITERAL_STRING(" "));
    id.Append(dumpID);

    // NotifyObservers is mainthread-only
    NS_DispatchToMainThread(WrapRunnableNM(&GMPNotifyObservers, id),
                            NS_DISPATCH_NORMAL);
  }
#endif
  // warn us off trying to close again
  mState = GMPStateClosing;
  mAbnormalShutdownInProgress = true;
  CloseActive(false);

  // Normal Shutdown() will delete the process on unwind.
  if (AbnormalShutdown == aWhy) {
    nsRefPtr<GMPParent> self(this);
    if (mAsyncShutdownRequired) {
      mService->AsyncShutdownComplete(this);
      mAsyncShutdownRequired = false;
    }
    // Must not call Close() again in DeleteProcess(), as we'll recurse
    // infinitely if we do.
    MOZ_ASSERT(mState == GMPStateClosing);
    DeleteProcess();
    // Note: final destruction will be Dispatched to ourself
    mService->ReAddOnGMPThread(self);
  }
}

bool
GMPParent::HasAccessedStorage() const
{
  return mHasAccessedStorage;
}

mozilla::dom::PCrashReporterParent*
GMPParent::AllocPCrashReporterParent(const NativeThreadId& aThread)
{
#ifndef MOZ_CRASHREPORTER
  MOZ_ASSERT(false, "Should only be sent if crash reporting is enabled.");
#endif
  CrashReporterParent* cr = new CrashReporterParent();
  cr->SetChildData(aThread, GeckoProcessType_GMPlugin);
  return cr;
}

bool
GMPParent::DeallocPCrashReporterParent(PCrashReporterParent* aCrashReporter)
{
  delete aCrashReporter;
  return true;
}

PGMPVideoDecoderParent*
GMPParent::AllocPGMPVideoDecoderParent()
{
  GMPVideoDecoderParent* vdp = new GMPVideoDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPParent::DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor)
{
  GMPVideoDecoderParent* vdp = static_cast<GMPVideoDecoderParent*>(aActor);
  NS_RELEASE(vdp);
  return true;
}

PGMPVideoEncoderParent*
GMPParent::AllocPGMPVideoEncoderParent()
{
  GMPVideoEncoderParent* vep = new GMPVideoEncoderParent(this);
  NS_ADDREF(vep);
  return vep;
}

bool
GMPParent::DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor)
{
  GMPVideoEncoderParent* vep = static_cast<GMPVideoEncoderParent*>(aActor);
  NS_RELEASE(vep);
  return true;
}

PGMPDecryptorParent*
GMPParent::AllocPGMPDecryptorParent()
{
  GMPDecryptorParent* ksp = new GMPDecryptorParent(this);
  NS_ADDREF(ksp);
  return ksp;
}

bool
GMPParent::DeallocPGMPDecryptorParent(PGMPDecryptorParent* aActor)
{
  GMPDecryptorParent* ksp = static_cast<GMPDecryptorParent*>(aActor);
  NS_RELEASE(ksp);
  return true;
}

PGMPAudioDecoderParent*
GMPParent::AllocPGMPAudioDecoderParent()
{
  GMPAudioDecoderParent* vdp = new GMPAudioDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPParent::DeallocPGMPAudioDecoderParent(PGMPAudioDecoderParent* aActor)
{
  GMPAudioDecoderParent* vdp = static_cast<GMPAudioDecoderParent*>(aActor);
  NS_RELEASE(vdp);
  return true;
}

PGMPStorageParent*
GMPParent::AllocPGMPStorageParent()
{
  GMPStorageParent* p = new GMPStorageParent(mNodeId, this);
  mStorage.AppendElement(p); // Addrefs, released in DeallocPGMPStorageParent.
  return p;
}

bool
GMPParent::DeallocPGMPStorageParent(PGMPStorageParent* aActor)
{
  GMPStorageParent* p = static_cast<GMPStorageParent*>(aActor);
  p->Shutdown();
  mStorage.RemoveElement(p);
  return true;
}

bool
GMPParent::RecvPGMPStorageConstructor(PGMPStorageParent* aActor)
{
  GMPStorageParent* p  = (GMPStorageParent*)aActor;
  if (NS_WARN_IF(NS_FAILED(p->Init()))) {
    return false;
  }
  mHasAccessedStorage = true;
  return true;
}

bool
GMPParent::RecvPGMPTimerConstructor(PGMPTimerParent* actor)
{
  return true;
}

PGMPTimerParent*
GMPParent::AllocPGMPTimerParent()
{
  GMPTimerParent* p = new GMPTimerParent(GMPThread());
  mTimers.AppendElement(p); // Released in DeallocPGMPTimerParent, or on shutdown.
  return p;
}

bool
GMPParent::DeallocPGMPTimerParent(PGMPTimerParent* aActor)
{
  GMPTimerParent* p = static_cast<GMPTimerParent*>(aActor);
  p->Shutdown();
  mTimers.RemoveElement(p);
  return true;
}

nsresult
ParseNextRecord(nsILineInputStream* aLineInputStream,
                const nsCString& aPrefix,
                nsCString& aResult,
                bool& aMoreLines)
{
  nsAutoCString record;
  nsresult rv = aLineInputStream->ReadLine(record, &aMoreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (record.Length() <= aPrefix.Length() ||
      !Substring(record, 0, aPrefix.Length()).Equals(aPrefix)) {
    return NS_ERROR_FAILURE;
  }

  aResult = Substring(record, aPrefix.Length());
  aResult.Trim("\b\t\r\n ");

  return NS_OK;
}

nsresult
GMPParent::ReadGMPMetaData()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(!mName.IsEmpty(), "Plugin mName cannot be empty!");

  nsCOMPtr<nsIFile> infoFile;
  nsresult rv = mDirectory->Clone(getter_AddRefs(infoFile));
  if (NS_FAILED(rv)) {
    return rv;
  }
  infoFile->AppendRelativePath(mName + NS_LITERAL_STRING(".info"));

  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), infoFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(inputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString value;
  bool moreLines = false;

  // 'Name:' record
  nsCString prefix = NS_LITERAL_CSTRING("Name:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (value.IsEmpty()) {
    // Not OK for name to be empty. Must have one non-whitespace character.
    return NS_ERROR_FAILURE;
  }
  mDisplayName = value;

  // 'Description:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Description:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mDescription = value;

  // 'Version:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Version:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mVersion = value;

  // 'Capability:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("APIs:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCCharSeparatedTokenizer apiTokens(value, ',');
  while (apiTokens.hasMoreTokens()) {
    nsAutoCString api(apiTokens.nextToken());
    api.StripWhitespace();
    if (api.IsEmpty()) {
      continue;
    }

    int32_t tagsStart = api.FindChar('[');
    if (tagsStart == 0) {
      // Not allowed to be the first character.
      // API name must be at least one character.
      continue;
    }

    auto cap = new GMPCapability();
    if (tagsStart == -1) {
      // No tags.
      cap->mAPIName.Assign(api);
    } else {
      auto tagsEnd = api.FindChar(']');
      if (tagsEnd == -1 || tagsEnd < tagsStart) {
        // Invalid syntax, skip whole capability.
        delete cap;
        continue;
      }

      cap->mAPIName.Assign(Substring(api, 0, tagsStart));

      if ((tagsEnd - tagsStart) > 1) {
        const nsDependentCSubstring ts(Substring(api, tagsStart + 1, tagsEnd - tagsStart - 1));
        nsCCharSeparatedTokenizer tagTokens(ts, ':');
        while (tagTokens.hasMoreTokens()) {
          const nsDependentCSubstring tag(tagTokens.nextToken());
          cap->mAPITags.AppendElement(tag);
        }
      }
    }

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
    if (cap->mAPIName.EqualsLiteral("eme-decrypt") &&
        !mozilla::CanSandboxMediaPlugin()) {
      printf_stderr("GMPParent::ReadGMPMetaData: Plugin \"%s\" is an EME CDM"
                    " but this system can't sandbox it; not loading.\n",
                    mDisplayName.get());
      delete cap;
      return NS_ERROR_FAILURE;
    }
#endif

    mCapabilities.AppendElement(cap);
  }

  if (mCapabilities.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
GMPParent::CanBeSharedCrossNodeIds() const
{
  return mNodeId.IsEmpty();
}

bool
GMPParent::CanBeUsedFrom(const nsACString& aNodeId) const
{
  return (mNodeId.IsEmpty() && State() == GMPStateNotLoaded) ||
         mNodeId == aNodeId;
}

void
GMPParent::SetNodeId(const nsACString& aNodeId)
{
  MOZ_ASSERT(!aNodeId.IsEmpty());
  MOZ_ASSERT(CanBeUsedFrom(aNodeId));
  mNodeId = aNodeId;
}

bool
GMPParent::RecvAsyncShutdownRequired()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  mAsyncShutdownRequired = true;
  mService->AsyncShutdownNeeded(this);
  return true;
}

bool
GMPParent::RecvAsyncShutdownComplete()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));

  MOZ_ASSERT(mAsyncShutdownRequired);
  AbortAsyncShutdown();
  return true;
}

} // namespace gmp
} // namespace mozilla

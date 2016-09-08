/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPParent.h"
#include "mozilla/Logging.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "nsIWritablePropertyBag2.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/SSE.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "GMPTimerParent.h"
#include "runnable_utils.h"
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/SandboxInfo.h"
#endif
#include "GMPContentParent.h"
#include "MediaPrefs.h"
#include "VideoUtils.h"

#include "mozilla/dom/CrashReporterParent.h"
using mozilla::dom::CrashReporterParent;
using mozilla::ipc::GeckoChildProcessHost;

#ifdef MOZ_CRASHREPORTER
#include "nsPrintfCString.h"
using CrashReporter::AnnotationTable;
using CrashReporter::GetIDFromMinidump;
#endif

#include "mozilla/Telemetry.h"

#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif

#include "mozilla/dom/WidevineCDMManifestBinding.h"
#include "widevine-adapter/WidevineAdapter.h"

namespace mozilla {

#undef LOG
#undef LOGD

extern LogModule* GetGMPLog();
#define LOG(level, x, ...) MOZ_LOG(GetGMPLog(), (level), (x, ##__VA_ARGS__))
#define LOGD(x, ...) LOG(mozilla::LogLevel::Debug, "GMPParent[%p|childPid=%d] " x, this, mChildPid, ##__VA_ARGS__)

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
  , mIsBlockingDeletion(false)
  , mCanDecrypt(false)
  , mGMPContentChildCount(0)
  , mAsyncShutdownRequired(false)
  , mAsyncShutdownInProgress(false)
  , mChildPid(0)
  , mHoldingSelfRef(false)
{
  mPluginId = GeckoChildProcessHost::GetUniqueID();
  LOGD("GMPParent ctor id=%u", mPluginId);
}

GMPParent::~GMPParent()
{
  LOGD("GMPParent dtor id=%u", mPluginId);
  MOZ_ASSERT(!mProcess);
}

nsresult
GMPParent::CloneFrom(const GMPParent* aOther)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  MOZ_ASSERT(aOther->mDirectory && aOther->mService, "null plugin directory");

  mService = aOther->mService;
  mDirectory = aOther->mDirectory;
  mName = aOther->mName;
  mVersion = aOther->mVersion;
  mDescription = aOther->mDescription;
  mDisplayName = aOther->mDisplayName;
#ifdef XP_WIN
  mLibs = aOther->mLibs;
#endif
  for (const GMPCapability& cap : aOther->mCapabilities) {
    mCapabilities.AppendElement(cap);
  }
  mAdapter = aOther->mAdapter;
  return NS_OK;
}

RefPtr<GenericPromise>
GMPParent::Init(GeckoMediaPluginServiceParent* aService, nsIFile* aPluginDir)
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
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  LOGD("%s: for %s", __FUNCTION__, NS_LossyConvertUTF16toASCII(parentLeafName).get());

  MOZ_ASSERT(parentLeafName.Length() > 4);
  mName = Substring(parentLeafName, 4);

  return ReadGMPMetaData();
}

void
GMPParent::Crash()
{
  if (mState != GMPStateNotLoaded) {
    Unused << SendCrashPluginNow();
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
  LOGD("%s: for %s", __FUNCTION__, NS_ConvertUTF16toUTF8(path).get());

  if (!mProcess) {
    mProcess = new GMPProcessParent(NS_ConvertUTF16toUTF8(path).get());
    if (!mProcess->Launch(30 * 1000)) {
      LOGD("%s: Failed to launch new child process", __FUNCTION__);
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }

    mChildPid = base::GetProcId(mProcess->GetChildProcessHandle());
    LOGD("%s: Launched new child process", __FUNCTION__);

    bool opened = Open(mProcess->GetChannel(),
                       base::GetProcId(mProcess->GetChildProcessHandle()));
    if (!opened) {
      LOGD("%s: Failed to open channel to new child process", __FUNCTION__);
      mProcess->Delete();
      mProcess = nullptr;
      return NS_ERROR_FAILURE;
    }
    LOGD("%s: Opened channel to new child process", __FUNCTION__);

    bool ok = SendSetNodeId(mNodeId);
    if (!ok) {
      LOGD("%s: Failed to send node id to child process", __FUNCTION__);
      return NS_ERROR_FAILURE;
    }
    LOGD("%s: Sent node id to child process", __FUNCTION__);

#ifdef XP_WIN
    if (!mLibs.IsEmpty()) {
      bool ok = SendPreloadLibs(mLibs);
      if (!ok) {
        LOGD("%s: Failed to send preload-libs to child process", __FUNCTION__);
        return NS_ERROR_FAILURE;
      }
      LOGD("%s: Sent preload-libs ('%s') to child process", __FUNCTION__, mLibs.get());
    }
#endif

    // Intr call to block initialization on plugin load.
    ok = CallStartPlugin(mAdapter);
    if (!ok) {
      LOGD("%s: Failed to send start to child process", __FUNCTION__);
      return NS_ERROR_FAILURE;
    }
    LOGD("%s: Sent StartPlugin to child process", __FUNCTION__);
  }

  mState = GMPStateLoaded;

  // Hold a self ref while the child process is alive. This ensures that
  // during shutdown the GMPParent stays alive long enough to
  // terminate the child process.
  MOZ_ASSERT(!mHoldingSelfRef);
  mHoldingSelfRef = true;
  AddRef();

  return NS_OK;
}

// static
void
GMPParent::AbortWaitingForGMPAsyncShutdown(nsITimer* aTimer, void* aClosure)
{
  NS_WARNING("Timed out waiting for GMP async shutdown!");
  GMPParent* parent = reinterpret_cast<GMPParent*>(aClosure);
  MOZ_ASSERT(parent->mService);
#if defined(MOZ_CRASHREPORTER)
  parent->mService->SetAsyncShutdownPluginState(parent, 'G',
    NS_LITERAL_CSTRING("Timed out waiting for async shutdown"));
#endif
  parent->mService->AsyncShutdownComplete(parent);
}

nsresult
GMPParent::EnsureAsyncShutdownTimeoutSet()
{
  MOZ_ASSERT(mAsyncShutdownRequired);
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

  int32_t timeout = MediaPrefs::GMPAsyncShutdownTimeout();
  RefPtr<GeckoMediaPluginServiceParent> service =
    GeckoMediaPluginServiceParent::GetSingleton();
  if (service) {
    timeout = service->AsyncShutdownTimeoutMs();
  }
  rv = mAsyncShutdownTimeout->InitWithFuncCallback(
    &AbortWaitingForGMPAsyncShutdown, this, timeout,
    nsITimer::TYPE_ONE_SHOT);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

bool
GMPParent::RecvPGMPContentChildDestroyed()
{
  --mGMPContentChildCount;
  if (!IsUsed()) {
#if defined(MOZ_CRASHREPORTER)
    if (mService) {
      mService->SetAsyncShutdownPluginState(this, 'E',
        NS_LITERAL_CSTRING("Last content child destroyed"));
    }
#endif
    CloseIfUnused();
  }
#if defined(MOZ_CRASHREPORTER)
  else {
    if (mService) {
      mService->SetAsyncShutdownPluginState(this, 'F',
        nsPrintfCString("Content child destroyed, remaining: %u", mGMPContentChildCount));
    }
  }
#endif
  return true;
}

void
GMPParent::CloseIfUnused()
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  LOGD("%s: mAsyncShutdownRequired=%d", __FUNCTION__, mAsyncShutdownRequired);

  if ((mDeleteProcessOnlyOnUnload ||
       mState == GMPStateLoaded ||
       mState == GMPStateUnloading) &&
      !IsUsed()) {
    // Ensure all timers are killed.
    for (uint32_t i = mTimers.Length(); i > 0; i--) {
      mTimers[i - 1]->Shutdown();
    }

    if (mAsyncShutdownRequired) {
      if (!mAsyncShutdownInProgress) {
        LOGD("%s: sending async shutdown notification", __FUNCTION__);
#if defined(MOZ_CRASHREPORTER)
        if (mService) {
          mService->SetAsyncShutdownPluginState(this, 'H',
            NS_LITERAL_CSTRING("Sent BeginAsyncShutdown"));
        }
#endif
        mAsyncShutdownInProgress = true;
        if (!SendBeginAsyncShutdown()) {
#if defined(MOZ_CRASHREPORTER)
          if (mService) {
            mService->SetAsyncShutdownPluginState(this, 'I',
              NS_LITERAL_CSTRING("Could not send BeginAsyncShutdown - Aborting async shutdown"));
          }
#endif
          AbortAsyncShutdown();
        } else if (NS_FAILED(EnsureAsyncShutdownTimeoutSet())) {
#if defined(MOZ_CRASHREPORTER)
          if (mService) {
            mService->SetAsyncShutdownPluginState(this, 'J',
              NS_LITERAL_CSTRING("Could not start timer after sending BeginAsyncShutdown - Aborting async shutdown"));
          }
#endif
          AbortAsyncShutdown();
        }
      }
    } else {
#if defined(MOZ_CRASHREPORTER)
      if (mService) {
        mService->SetAsyncShutdownPluginState(this, 'K',
          NS_LITERAL_CSTRING("No (more) async-shutdown required"));
      }
#endif
      // No async-shutdown, kill async-shutdown timer started in CloseActive().
      AbortAsyncShutdown();
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
  LOGD("%s", __FUNCTION__);

  if (mAsyncShutdownTimeout) {
    mAsyncShutdownTimeout->Cancel();
    mAsyncShutdownTimeout = nullptr;
  }

  if (!mAsyncShutdownRequired || !mAsyncShutdownInProgress) {
    return;
  }

  RefPtr<GMPParent> kungFuDeathGrip(this);
  mService->AsyncShutdownComplete(this);
  mAsyncShutdownRequired = false;
  mAsyncShutdownInProgress = false;
  CloseIfUnused();
}

void
GMPParent::CloseActive(bool aDieWhenUnloaded)
{
  LOGD("%s: state %d", __FUNCTION__, mState);
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (aDieWhenUnloaded) {
    mDeleteProcessOnlyOnUnload = true; // don't allow this to go back...
  }
  if (mState == GMPStateLoaded) {
    mState = GMPStateUnloading;
  }
  if (mState != GMPStateNotLoaded && IsUsed()) {
#if defined(MOZ_CRASHREPORTER)
    if (mService) {
      mService->SetAsyncShutdownPluginState(this, 'A',
        nsPrintfCString("Sent CloseActive, content children to close: %u", mGMPContentChildCount));
    }
#endif
    if (!SendCloseActive()) {
#if defined(MOZ_CRASHREPORTER)
      if (mService) {
        mService->SetAsyncShutdownPluginState(this, 'B',
          NS_LITERAL_CSTRING("Could not send CloseActive - Aborting async shutdown"));
      }
#endif
      AbortAsyncShutdown();
    } else if (IsUsed()) {
      // We're expecting RecvPGMPContentChildDestroyed's -> Start async-shutdown timer now if needed.
      if (mAsyncShutdownRequired && NS_FAILED(EnsureAsyncShutdownTimeoutSet())) {
#if defined(MOZ_CRASHREPORTER)
        if (mService) {
          mService->SetAsyncShutdownPluginState(this, 'C',
            NS_LITERAL_CSTRING("Could not start timer after sending CloseActive - Aborting async shutdown"));
        }
#endif
        AbortAsyncShutdown();
      }
    } else {
      // We're not expecting any RecvPGMPContentChildDestroyed
      // -> Call CloseIfUnused() now, to run async shutdown if necessary.
      // Note that CloseIfUnused() may have already been called from a prior
      // RecvPGMPContentChildDestroyed(), however depending on the state at
      // that time, it might not have proceeded with shutdown; And calling it
      // again after shutdown is fine because after the first one we'll be in
      // GMPStateNotLoaded.
#if defined(MOZ_CRASHREPORTER)
      if (mService) {
        mService->SetAsyncShutdownPluginState(this, 'D',
          NS_LITERAL_CSTRING("Content children already destroyed"));
      }
#endif
      CloseIfUnused();
    }
  }
}

void
GMPParent::MarkForDeletion()
{
  mDeleteProcessOnlyOnUnload = true;
  mIsBlockingDeletion = true;
}

bool
GMPParent::IsMarkedForDeletion()
{
  return mIsBlockingDeletion;
}

void
GMPParent::Shutdown()
{
  LOGD("%s", __FUNCTION__);
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ASSERT(!mAsyncShutdownTimeout, "Should have canceled shutdown timeout");

  if (mAbnormalShutdownInProgress) {
    return;
  }

  MOZ_ASSERT(!IsUsed());
  if (mState == GMPStateNotLoaded || mState == GMPStateClosing) {
    return;
  }

  RefPtr<GMPParent> self(this);
  DeleteProcess();

  // XXX Get rid of mDeleteProcessOnlyOnUnload and this code when
  // Bug 1043671 is fixed
  if (!mDeleteProcessOnlyOnUnload) {
    // Destroy ourselves and rise from the fire to save memory
    mService->ReAddOnGMPThread(self);
  } // else we've been asked to die and stay dead
  MOZ_ASSERT(mState == GMPStateNotLoaded);
}

class NotifyGMPShutdownTask : public Runnable {
public:
  explicit NotifyGMPShutdownTask(const nsAString& aNodeId)
    : mNodeId(aNodeId)
  {
  }
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
    MOZ_ASSERT(obsService);
    if (obsService) {
      obsService->NotifyObservers(nullptr, "gmp-shutdown", mNodeId.get());
    }
    return NS_OK;
  }
  nsString mNodeId;
};

void
GMPParent::ChildTerminated()
{
  RefPtr<GMPParent> self(this);
  nsIThread* gmpThread = GMPThread();

  if (!gmpThread) {
    // Bug 1163239 - this can happen on shutdown.
    // PluginTerminated removes the GMP from the GMPService.
    // On shutdown we can have this case where it is already been
    // removed so there is no harm in not trying to remove it again.
    LOGD("%s::%s: GMPThread() returned nullptr.", __CLASS__, __FUNCTION__);
  } else {
    gmpThread->Dispatch(NewRunnableMethod<RefPtr<GMPParent>>(
                         mService,
                         &GeckoMediaPluginServiceParent::PluginTerminated,
                         self),
                         NS_DISPATCH_NORMAL);
  }
}

void
GMPParent::DeleteProcess()
{
  LOGD("%s", __FUNCTION__);

  if (mState != GMPStateClosing) {
    // Don't Close() twice!
    // Probably remove when bug 1043671 is resolved
    mState = GMPStateClosing;
    Close();
  }
  mProcess->Delete(NewRunnableMethod(this, &GMPParent::ChildTerminated));
  LOGD("%s: Shut down process", __FUNCTION__);
  mProcess = nullptr;
  mState = GMPStateNotLoaded;

  NS_DispatchToMainThread(
    new NotifyGMPShutdownTask(NS_ConvertUTF8toUTF16(mNodeId)),
    NS_DISPATCH_NORMAL);

  if (mHoldingSelfRef) {
    Release();
    mHoldingSelfRef = false;
  }
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
    if (!mCapabilities[i].mAPIName.Equals(aAPI)) {
      continue;
    }
    nsTArray<nsCString>& tags = mCapabilities[i].mAPITags;
    for (uint32_t j = 0; j < tags.Length(); j++) {
      if (tags[j].Equals(aTag)) {
#ifdef XP_WIN
        // Clearkey on Windows advertises that it can decode in its GMP info
        // file, but uses Windows Media Foundation to decode. That's not present
        // on Windows XP, and on some Vista, Windows N, and KN variants without
        // certain services packs.
        if (tags[j].Equals(kEMEKeySystemClearkey)) {
          if (mCapabilities[i].mAPIName.EqualsLiteral(GMP_API_VIDEO_DECODER)) {
            if (!WMFDecoderModule::HasH264()) {
              continue;
            }
          } else if (mCapabilities[i].mAPIName.EqualsLiteral(GMP_API_AUDIO_DECODER)) {
            if (!WMFDecoderModule::HasAAC()) {
              continue;
            }
          }
        }
#endif
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
  CrashReporterParent* cr =
    static_cast<CrashReporterParent*>(LoneManagedOrNullAsserts(ManagedPCrashReporterParent()));
  if (NS_WARN_IF(!cr)) {
    return;
  }

  AnnotationTable notes(4);
  WriteExtraDataForMinidump(notes);
  nsCOMPtr<nsIFile> dumpFile;
  TakeMinidump(getter_AddRefs(dumpFile), nullptr);
  if (!dumpFile) {
    NS_WARNING("GMP crash without crash report");
    aResult = mName;
    aResult += '-';
    AppendUTF8toUTF16(mVersion, aResult);
    return;
  }
  GetIDFromMinidump(dumpFile, aResult);
  cr->GenerateCrashReportForMinidump(dumpFile, &notes);
}

static void
GMPNotifyObservers(const uint32_t aPluginID, const nsACString& aPluginName, const nsAString& aPluginDumpID)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  nsCOMPtr<nsIWritablePropertyBag2> propbag =
    do_CreateInstance("@mozilla.org/hash-property-bag;1");
  if (obs && propbag) {
    propbag->SetPropertyAsUint32(NS_LITERAL_STRING("pluginID"), aPluginID);
    propbag->SetPropertyAsACString(NS_LITERAL_STRING("pluginName"), aPluginName);
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("pluginDumpID"), aPluginDumpID);
    obs->NotifyObservers(propbag, "gmp-plugin-crash", nullptr);
  }

  RefPtr<gmp::GeckoMediaPluginService> service =
    gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
  if (service) {
    service->RunPluginCrashCallbacks(aPluginID, aPluginName);
  }
}
#endif
void
GMPParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD("%s: (%d)", __FUNCTION__, (int)aWhy);
#ifdef MOZ_CRASHREPORTER
  if (AbnormalShutdown == aWhy) {
    Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
                          NS_LITERAL_CSTRING("gmplugin"), 1);
    nsString dumpID;
    GetCrashID(dumpID);

    // NotifyObservers is mainthread-only
    NS_DispatchToMainThread(WrapRunnableNM(&GMPNotifyObservers,
                                           mPluginId, mDisplayName, dumpID),
                            NS_DISPATCH_NORMAL);
  }
#endif
  // warn us off trying to close again
  mState = GMPStateClosing;
  mAbnormalShutdownInProgress = true;
  CloseActive(false);

  // Normal Shutdown() will delete the process on unwind.
  if (AbnormalShutdown == aWhy) {
    RefPtr<GMPParent> self(this);
    if (mAsyncShutdownRequired) {
#if defined(MOZ_CRASHREPORTER)
      if (mService) {
        mService->SetAsyncShutdownPluginState(this, 'M',
          NS_LITERAL_CSTRING("Actor destroyed"));
      }
#endif
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

bool
ReadInfoField(GMPInfoFileParser& aParser, const nsCString& aKey, nsACString& aOutValue)
{
  if (!aParser.Contains(aKey) || aParser.Get(aKey).IsEmpty()) {
    return false;
  }
  aOutValue = aParser.Get(aKey);
  return true;
}

RefPtr<GenericPromise>
GMPParent::ReadGMPMetaData()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(!mName.IsEmpty(), "Plugin mName cannot be empty!");

  nsCOMPtr<nsIFile> infoFile;
  nsresult rv = mDirectory->Clone(getter_AddRefs(infoFile));
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  infoFile->AppendRelativePath(mName + NS_LITERAL_STRING(".info"));

  if (FileExists(infoFile)) {
    return ReadGMPInfoFile(infoFile);
  }

  // Maybe this is the Widevine adapted plugin?
  nsCOMPtr<nsIFile> manifestFile;
  rv = mDirectory->Clone(getter_AddRefs(manifestFile));
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  manifestFile->AppendRelativePath(NS_LITERAL_STRING("manifest.json"));
  return ReadChromiumManifestFile(manifestFile);
}

RefPtr<GenericPromise>
GMPParent::ReadGMPInfoFile(nsIFile* aFile)
{
  GMPInfoFileParser parser;
  if (!parser.Init(aFile)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsAutoCString apis;
  if (!ReadInfoField(parser, NS_LITERAL_CSTRING("name"), mDisplayName) ||
      !ReadInfoField(parser, NS_LITERAL_CSTRING("description"), mDescription) ||
      !ReadInfoField(parser, NS_LITERAL_CSTRING("version"), mVersion) ||
      !ReadInfoField(parser, NS_LITERAL_CSTRING("apis"), apis)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

#ifdef XP_WIN
  // "Libraries" field is optional.
  ReadInfoField(parser, NS_LITERAL_CSTRING("libraries"), mLibs);
#endif

  nsTArray<nsCString> apiTokens;
  SplitAt(", ", apis, apiTokens);
  for (nsCString api : apiTokens) {
    int32_t tagsStart = api.FindChar('[');
    if (tagsStart == 0) {
      // Not allowed to be the first character.
      // API name must be at least one character.
      continue;
    }

    GMPCapability cap;
    if (tagsStart == -1) {
      // No tags.
      cap.mAPIName.Assign(api);
    } else {
      auto tagsEnd = api.FindChar(']');
      if (tagsEnd == -1 || tagsEnd < tagsStart) {
        // Invalid syntax, skip whole capability.
        continue;
      }

      cap.mAPIName.Assign(Substring(api, 0, tagsStart));

      if ((tagsEnd - tagsStart) > 1) {
        const nsDependentCSubstring ts(Substring(api, tagsStart + 1, tagsEnd - tagsStart - 1));
        nsTArray<nsCString> tagTokens;
        SplitAt(":", ts, tagTokens);
        for (nsCString tag : tagTokens) {
          cap.mAPITags.AppendElement(tag);
        }
      }
    }

    // We support the current GMPDecryptor version, and the previous.
    // We Adapt the previous to the current in the GMPContentChild.
    if (cap.mAPIName.EqualsLiteral(GMP_API_DECRYPTOR_BACKWARDS_COMPAT)) {
      cap.mAPIName.AssignLiteral(GMP_API_DECRYPTOR);
    }

    if (cap.mAPIName.EqualsLiteral(GMP_API_DECRYPTOR)) {
      mCanDecrypt = true;

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
      if (!mozilla::SandboxInfo::Get().CanSandboxMedia()) {
        printf_stderr("GMPParent::ReadGMPMetaData: Plugin \"%s\" is an EME CDM"
                      " but this system can't sandbox it; not loading.\n",
                      mDisplayName.get());
        return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
      }
#endif
#ifdef XP_WIN
      // Adobe GMP doesn't work without SSE2. Check the tags to see if
      // the decryptor is for the Adobe GMP, and refuse to load it if
      // SSE2 isn't supported.
      if (cap.mAPITags.Contains(kEMEKeySystemPrimetime) &&
          !mozilla::supports_sse2()) {
        return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
      }
#endif // XP_WIN
    }

    mCapabilities.AppendElement(Move(cap));
  }

  if (mCapabilities.IsEmpty()) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

RefPtr<GenericPromise>
GMPParent::ReadChromiumManifestFile(nsIFile* aFile)
{
  nsAutoCString json;
  if (!ReadIntoString(aFile, json, 5 * 1024)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // DOM JSON parsing needs to run on the main thread.
  return InvokeAsync(AbstractThread::MainThread(), this, __func__,
    &GMPParent::ParseChromiumManifest, NS_ConvertUTF8toUTF16(json));
}

RefPtr<GenericPromise>
GMPParent::ParseChromiumManifest(nsString aJSON)
{
  LOGD("%s: for '%s'", __FUNCTION__, NS_LossyConvertUTF16toASCII(aJSON).get());

  MOZ_ASSERT(NS_IsMainThread());
  mozilla::dom::WidevineCDMManifest m;
  if (!m.Init(aJSON)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsresult ignored; // Note: ToInteger returns 0 on failure.
  if (!WidevineAdapter::Supports(m.mX_cdm_module_versions.ToInteger(&ignored),
                                 m.mX_cdm_interface_versions.ToInteger(&ignored),
                                 m.mX_cdm_host_versions.ToInteger(&ignored))) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mDisplayName = NS_ConvertUTF16toUTF8(m.mName);
  mDescription = NS_ConvertUTF16toUTF8(m.mDescription);
  mVersion = NS_ConvertUTF16toUTF8(m.mVersion);

  GMPCapability video(NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER));
  video.mAPITags.AppendElement(NS_LITERAL_CSTRING("h264"));
  video.mAPITags.AppendElement(NS_LITERAL_CSTRING("vp8"));
  video.mAPITags.AppendElement(NS_LITERAL_CSTRING("vp9"));
  video.mAPITags.AppendElement(kEMEKeySystemWidevine);
  mCapabilities.AppendElement(Move(video));

  GMPCapability decrypt(NS_LITERAL_CSTRING(GMP_API_DECRYPTOR));
  decrypt.mAPITags.AppendElement(kEMEKeySystemWidevine);
  mCapabilities.AppendElement(Move(decrypt));

  MOZ_ASSERT(mName.EqualsLiteral("widevinecdm"));
  mAdapter = NS_LITERAL_STRING("widevine");
#ifdef XP_WIN
  mLibs = NS_LITERAL_CSTRING("dxva2.dll");
#endif

  return GenericPromise::CreateAndResolve(true, __func__);
}

bool
GMPParent::CanBeSharedCrossNodeIds() const
{
  return !mAsyncShutdownInProgress &&
         mNodeId.IsEmpty() &&
         // XXX bug 1159300 hack -- maybe remove after openh264 1.4
         // We don't want to use CDM decoders for non-encrypted playback
         // just yet; especially not for WebRTC. Don't allow CDMs to be used
         // without a node ID.
         !mCanDecrypt;
}

bool
GMPParent::CanBeUsedFrom(const nsACString& aNodeId) const
{
  return !mAsyncShutdownInProgress && mNodeId == aNodeId;
}

void
GMPParent::SetNodeId(const nsACString& aNodeId)
{
  MOZ_ASSERT(!aNodeId.IsEmpty());
  mNodeId = aNodeId;
}

const nsCString&
GMPParent::GetDisplayName() const
{
  return mDisplayName;
}

const nsCString&
GMPParent::GetVersion() const
{
  return mVersion;
}

uint32_t
GMPParent::GetPluginId() const
{
  return mPluginId;
}

bool
GMPParent::RecvAsyncShutdownRequired()
{
  LOGD("%s", __FUNCTION__);
  if (mAsyncShutdownRequired) {
    NS_WARNING("Received AsyncShutdownRequired message more than once!");
    return true;
  }
  mAsyncShutdownRequired = true;
  mService->AsyncShutdownNeeded(this);
  return true;
}

bool
GMPParent::RecvAsyncShutdownComplete()
{
  LOGD("%s", __FUNCTION__);

  MOZ_ASSERT(mAsyncShutdownRequired);
#if defined(MOZ_CRASHREPORTER)
  if (mService) {
    mService->SetAsyncShutdownPluginState(this, 'L',
      NS_LITERAL_CSTRING("Received AsyncShutdownComplete"));
  }
#endif
  AbortAsyncShutdown();
  return true;
}

class RunCreateContentParentCallbacks : public Runnable
{
public:
  explicit RunCreateContentParentCallbacks(GMPContentParent* aGMPContentParent)
    : mGMPContentParent(aGMPContentParent)
  {
  }

  void TakeCallbacks(nsTArray<UniquePtr<GetGMPContentParentCallback>>& aCallbacks)
  {
    mCallbacks.SwapElements(aCallbacks);
  }

  NS_IMETHOD
  Run() override
  {
    for (uint32_t i = 0, length = mCallbacks.Length(); i < length; ++i) {
      mCallbacks[i]->Done(mGMPContentParent);
    }
    return NS_OK;
  }

private:
  RefPtr<GMPContentParent> mGMPContentParent;
  nsTArray<UniquePtr<GetGMPContentParentCallback>> mCallbacks;
};

PGMPContentParent*
GMPParent::AllocPGMPContentParent(Transport* aTransport, ProcessId aOtherPid)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());
  MOZ_ASSERT(!mGMPContentParent);

  mGMPContentParent = new GMPContentParent(this);
  mGMPContentParent->Open(aTransport, aOtherPid, XRE_GetIOMessageLoop(),
                          ipc::ParentSide);

  RefPtr<RunCreateContentParentCallbacks> runCallbacks =
    new RunCreateContentParentCallbacks(mGMPContentParent);
  runCallbacks->TakeCallbacks(mCallbacks);
  NS_DispatchToCurrentThread(runCallbacks);
  MOZ_ASSERT(mCallbacks.IsEmpty());

  return mGMPContentParent;
}

bool
GMPParent::GetGMPContentParent(UniquePtr<GetGMPContentParentCallback>&& aCallback)
{
  LOGD("%s %p", __FUNCTION__, this);
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (mGMPContentParent) {
    aCallback->Done(mGMPContentParent);
  } else {
    mCallbacks.AppendElement(Move(aCallback));
    // If we don't have a GMPContentParent and we try to get one for the first
    // time (mCallbacks.Length() == 1) then call PGMPContent::Open. If more
    // calls to GetGMPContentParent happen before mGMPContentParent has been
    // set then we should just store them, so that they get called when we set
    // mGMPContentParent as a result of the PGMPContent::Open call.
    if (mCallbacks.Length() == 1) {
      if (!EnsureProcessLoaded() || !PGMPContent::Open(this)) {
        return false;
      }
      // We want to increment this as soon as possible, to avoid that we'd try
      // to shut down the GMP process while we're still trying to get a
      // PGMPContentParent actor.
      ++mGMPContentChildCount;
    }
  }
  return true;
}

already_AddRefed<GMPContentParent>
GMPParent::ForgetGMPContentParent()
{
  MOZ_ASSERT(mCallbacks.IsEmpty());
  return Move(mGMPContentParent.forget());
}

bool
GMPParent::EnsureProcessLoaded(base::ProcessId* aID)
{
  if (!EnsureProcessLoaded()) {
    return false;
  }
  *aID = OtherPid();
  return true;
}

bool
GMPParent::Bridge(GMPServiceParent* aGMPServiceParent)
{
  if (NS_FAILED(PGMPContent::Bridge(aGMPServiceParent, this))) {
    return false;
  }
  ++mGMPContentChildCount;
  return true;
}

nsString
GMPParent::GetPluginBaseName() const
{
  return NS_LITERAL_STRING("gmp-") + mName;
}

} // namespace gmp
} // namespace mozilla

#undef LOG
#undef LOGD

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPParent.h"
#include "mozilla/Logging.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "nsIWritablePropertyBag2.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ipc/CrashReporterHost.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/SSE.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "GMPTimerParent.h"
#include "runnable_utils.h"
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxInfo.h"
#endif
#include "CDMStorageIdProvider.h"
#include "GMPContentParent.h"
#include "VideoUtils.h"

using mozilla::ipc::GeckoChildProcessHost;

using CrashReporter::AnnotationTable;
using CrashReporter::GetIDFromMinidump;

#include "mozilla/Telemetry.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#endif

#include "mozilla/dom/WidevineCDMManifestBinding.h"
#include "ChromiumCDMAdapter.h"

namespace mozilla {

#undef LOG
#undef LOGD

extern LogModule* GetGMPLog();
#define LOG(level, x, ...) MOZ_LOG(GetGMPLog(), (level), (x, ##__VA_ARGS__))
#define LOGD(x, ...)                                                  \
  LOG(mozilla::LogLevel::Debug, "GMPParent[%p|childPid=%d] " x, this, \
      mChildPid, ##__VA_ARGS__)

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPParent"

namespace gmp {

GMPParent::GMPParent(AbstractThread* aMainThread)
    : mState(GMPStateNotLoaded),
      mProcess(nullptr),
      mDeleteProcessOnlyOnUnload(false),
      mAbnormalShutdownInProgress(false),
      mIsBlockingDeletion(false),
      mCanDecrypt(false),
      mGMPContentChildCount(0),
      mChildPid(0),
      mHoldingSelfRef(false),
      mMainThread(aMainThread) {
  mPluginId = GeckoChildProcessHost::GetUniqueID();
  LOGD("GMPParent ctor id=%u", mPluginId);
}

GMPParent::~GMPParent() {
  LOGD("GMPParent dtor id=%u", mPluginId);
  MOZ_ASSERT(!mProcess);
}

nsresult GMPParent::CloneFrom(const GMPParent* aOther) {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
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

RefPtr<GenericPromise> GMPParent::Init(GeckoMediaPluginServiceParent* aService,
                                       nsIFile* aPluginDir) {
  MOZ_ASSERT(aPluginDir);
  MOZ_ASSERT(aService);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  mService = aService;
  mDirectory = aPluginDir;

  // aPluginDir is <profile-dir>/<gmp-plugin-id>/<version>
  // where <gmp-plugin-id> should be gmp-gmpopenh264
  nsCOMPtr<nsIFile> parent;
  nsresult rv = aPluginDir->GetParent(getter_AddRefs(parent));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  nsAutoString parentLeafName;
  rv = parent->GetLeafName(parentLeafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  LOGD("%s: for %s", __FUNCTION__,
       NS_LossyConvertUTF16toASCII(parentLeafName).get());

  MOZ_ASSERT(parentLeafName.Length() > 4);
  mName = Substring(parentLeafName, 4);

  return ReadGMPMetaData();
}

void GMPParent::Crash() {
  if (mState != GMPStateNotLoaded) {
    Unused << SendCrashPluginNow();
  }
}

nsresult GMPParent::LoadProcess() {
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(mState == GMPStateNotLoaded);

  nsAutoString path;
  if (NS_WARN_IF(NS_FAILED(mDirectory->GetPath(path)))) {
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

    // ComputeStorageId may return empty string, we leave the error handling to
    // CDM. The CDM will reject the promise once we provide a empty string of
    // storage id.
    bool ok =
        SendProvideStorageId(CDMStorageIdProvider::ComputeStorageId(mNodeId));
    if (!ok) {
      LOGD("%s: Failed to send storage id to child process", __FUNCTION__);
      return NS_ERROR_FAILURE;
    }
    LOGD("%s: Sent storage id to child process", __FUNCTION__);

#ifdef XP_WIN
    if (!mLibs.IsEmpty()) {
      bool ok = SendPreloadLibs(mLibs);
      if (!ok) {
        LOGD("%s: Failed to send preload-libs to child process", __FUNCTION__);
        return NS_ERROR_FAILURE;
      }
      LOGD("%s: Sent preload-libs ('%s') to child process", __FUNCTION__,
           mLibs.get());
    }
#endif

    // Intr call to block initialization on plugin load.
    if (!CallStartPlugin(mAdapter)) {
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

mozilla::ipc::IPCResult GMPParent::RecvPGMPContentChildDestroyed() {
  --mGMPContentChildCount;
  if (!IsUsed()) {
    CloseIfUnused();
  }
  return IPC_OK();
}

void GMPParent::CloseIfUnused() {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
  LOGD("%s", __FUNCTION__);

  if ((mDeleteProcessOnlyOnUnload || mState == GMPStateLoaded ||
       mState == GMPStateUnloading) &&
      !IsUsed()) {
    // Ensure all timers are killed.
    for (uint32_t i = mTimers.Length(); i > 0; i--) {
      mTimers[i - 1]->Shutdown();
    }

    // Shutdown GMPStorage. Given that all protocol actors must be shutdown
    // (!Used() is true), all storage operations should be complete.
    for (size_t i = mStorage.Length(); i > 0; i--) {
      mStorage[i - 1]->Shutdown();
    }
    Shutdown();
  }
}

void GMPParent::CloseActive(bool aDieWhenUnloaded) {
  LOGD("%s: state %d", __FUNCTION__, mState);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  if (aDieWhenUnloaded) {
    mDeleteProcessOnlyOnUnload = true;  // don't allow this to go back...
  }
  if (mState == GMPStateLoaded) {
    mState = GMPStateUnloading;
  }
  if (mState != GMPStateNotLoaded && IsUsed()) {
    Unused << SendCloseActive();
    CloseIfUnused();
  }
}

void GMPParent::MarkForDeletion() {
  mDeleteProcessOnlyOnUnload = true;
  mIsBlockingDeletion = true;
}

bool GMPParent::IsMarkedForDeletion() { return mIsBlockingDeletion; }

void GMPParent::Shutdown() {
  LOGD("%s", __FUNCTION__);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

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
  }  // else we've been asked to die and stay dead
  MOZ_ASSERT(mState == GMPStateNotLoaded);
}

class NotifyGMPShutdownTask : public Runnable {
 public:
  explicit NotifyGMPShutdownTask(const nsAString& aNodeId)
      : Runnable("NotifyGMPShutdownTask"), mNodeId(aNodeId) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(obsService);
    if (obsService) {
      obsService->NotifyObservers(nullptr, "gmp-shutdown", mNodeId.get());
    }
    return NS_OK;
  }
  nsString mNodeId;
};

void GMPParent::ChildTerminated() {
  RefPtr<GMPParent> self(this);
  nsCOMPtr<nsISerialEventTarget> gmpEventTarget = GMPEventTarget();

  if (!gmpEventTarget) {
    // Bug 1163239 - this can happen on shutdown.
    // PluginTerminated removes the GMP from the GMPService.
    // On shutdown we can have this case where it is already been
    // removed so there is no harm in not trying to remove it again.
    LOGD("%s::%s: GMPEventTarget() returned nullptr.", __CLASS__, __FUNCTION__);
  } else {
    gmpEventTarget->Dispatch(
        NewRunnableMethod<RefPtr<GMPParent>>(
            "gmp::GeckoMediaPluginServiceParent::PluginTerminated", mService,
            &GeckoMediaPluginServiceParent::PluginTerminated, self),
        NS_DISPATCH_NORMAL);
  }
}

void GMPParent::DeleteProcess() {
  LOGD("%s", __FUNCTION__);

  if (mState != GMPStateClosing) {
    // Don't Close() twice!
    // Probably remove when bug 1043671 is resolved
    mState = GMPStateClosing;
    Close();
  }
  mProcess->Delete(NewRunnableMethod("gmp::GMPParent::ChildTerminated", this,
                                     &GMPParent::ChildTerminated));
  LOGD("%s: Shut down process", __FUNCTION__);
  mProcess = nullptr;
  mState = GMPStateNotLoaded;

  nsCOMPtr<nsIRunnable> r =
      new NotifyGMPShutdownTask(NS_ConvertUTF8toUTF16(mNodeId));
  mMainThread->Dispatch(r.forget());

  if (mHoldingSelfRef) {
    Release();
    mHoldingSelfRef = false;
  }
}

GMPState GMPParent::State() const { return mState; }

nsCOMPtr<nsISerialEventTarget> GMPParent::GMPEventTarget() {
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
      do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  MOZ_ASSERT(mps);
  if (!mps) {
    return nullptr;
  }
  // Note: GeckoMediaPluginService::GetThread() is threadsafe, and returns
  // nullptr if the GeckoMediaPluginService has started shutdown.
  nsCOMPtr<nsIThread> gmpThread;
  mps->GetThread(getter_AddRefs(gmpThread));
  return gmpThread ? gmpThread->SerialEventTarget() : nullptr;
}

/* static */
bool GMPCapability::Supports(const nsTArray<GMPCapability>& aCapabilities,
                             const nsCString& aAPI,
                             const nsTArray<nsCString>& aTags) {
  for (const nsCString& tag : aTags) {
    if (!GMPCapability::Supports(aCapabilities, aAPI, tag)) {
      return false;
    }
  }
  return true;
}

/* static */
bool GMPCapability::Supports(const nsTArray<GMPCapability>& aCapabilities,
                             const nsCString& aAPI, const nsCString& aTag) {
  for (const GMPCapability& capabilities : aCapabilities) {
    if (!capabilities.mAPIName.Equals(aAPI)) {
      continue;
    }
    for (const nsCString& tag : capabilities.mAPITags) {
      if (tag.Equals(aTag)) {
#ifdef XP_WIN
        // Clearkey on Windows advertises that it can decode in its GMP info
        // file, but uses Windows Media Foundation to decode. That's not present
        // on Windows XP, and on some Vista, Windows N, and KN variants without
        // certain services packs.
        if (tag.EqualsLiteral(EME_KEY_SYSTEM_CLEARKEY)) {
          if (capabilities.mAPIName.EqualsLiteral(GMP_API_VIDEO_DECODER)) {
            if (!WMFDecoderModule::HasH264()) {
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

bool GMPParent::EnsureProcessLoaded() {
  if (mState == GMPStateLoaded) {
    return true;
  }
  if (mState == GMPStateClosing || mState == GMPStateUnloading) {
    return false;
  }

  nsresult rv = LoadProcess();

  return NS_SUCCEEDED(rv);
}

void GMPParent::WriteExtraDataForMinidump() {
  mCrashReporter->AddAnnotation(CrashReporter::Annotation::GMPPlugin, true);
  mCrashReporter->AddAnnotation(CrashReporter::Annotation::PluginFilename,
                                NS_ConvertUTF16toUTF8(mName));
  mCrashReporter->AddAnnotation(CrashReporter::Annotation::PluginName,
                                mDisplayName);
  mCrashReporter->AddAnnotation(CrashReporter::Annotation::PluginVersion,
                                mVersion);
}

bool GMPParent::GetCrashID(nsString& aResult) {
  if (!mCrashReporter) {
    return false;
  }

  WriteExtraDataForMinidump();
  if (!mCrashReporter->GenerateCrashReport(OtherPid())) {
    return false;
  }

  aResult = mCrashReporter->MinidumpID();
  return true;
}

static void GMPNotifyObservers(const uint32_t aPluginID,
                               const nsACString& aPluginName,
                               const nsAString& aPluginDumpID) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  nsCOMPtr<nsIWritablePropertyBag2> propbag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");
  if (obs && propbag) {
    propbag->SetPropertyAsUint32(NS_LITERAL_STRING("pluginID"), aPluginID);
    propbag->SetPropertyAsACString(NS_LITERAL_STRING("pluginName"),
                                   aPluginName);
    propbag->SetPropertyAsAString(NS_LITERAL_STRING("pluginDumpID"),
                                  aPluginDumpID);
    obs->NotifyObservers(propbag, "gmp-plugin-crash", nullptr);
  }

  RefPtr<gmp::GeckoMediaPluginService> service =
      gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
  if (service) {
    service->RunPluginCrashCallbacks(aPluginID, aPluginName);
  }
}

void GMPParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOGD("%s: (%d)", __FUNCTION__, (int)aWhy);

  if (AbnormalShutdown == aWhy) {
    Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
                          NS_LITERAL_CSTRING("gmplugin"), 1);
    nsString dumpID;
    if (!GetCrashID(dumpID)) {
      NS_WARNING("GMP crash without crash report");
      dumpID = mName;
      dumpID += '-';
      AppendUTF8toUTF16(mVersion, dumpID);
    }

    // NotifyObservers is mainthread-only
    nsCOMPtr<nsIRunnable> r =
        WrapRunnableNM(&GMPNotifyObservers, mPluginId, mDisplayName, dumpID);
    mMainThread->Dispatch(r.forget());
  }

  // warn us off trying to close again
  mState = GMPStateClosing;
  mAbnormalShutdownInProgress = true;
  CloseActive(false);

  // Normal Shutdown() will delete the process on unwind.
  if (AbnormalShutdown == aWhy) {
    RefPtr<GMPParent> self(this);
    // Must not call Close() again in DeleteProcess(), as we'll recurse
    // infinitely if we do.
    MOZ_ASSERT(mState == GMPStateClosing);
    DeleteProcess();
    // Note: final destruction will be Dispatched to ourself
    mService->ReAddOnGMPThread(self);
  }
}

mozilla::ipc::IPCResult GMPParent::RecvInitCrashReporter(
    Shmem&& aShmem, const NativeThreadId& aThreadId) {
  mCrashReporter = MakeUnique<ipc::CrashReporterHost>(GeckoProcessType_GMPlugin,
                                                      aShmem, aThreadId);

  return IPC_OK();
}

PGMPStorageParent* GMPParent::AllocPGMPStorageParent() {
  GMPStorageParent* p = new GMPStorageParent(mNodeId, this);
  mStorage.AppendElement(p);  // Addrefs, released in DeallocPGMPStorageParent.
  return p;
}

bool GMPParent::DeallocPGMPStorageParent(PGMPStorageParent* aActor) {
  GMPStorageParent* p = static_cast<GMPStorageParent*>(aActor);
  p->Shutdown();
  mStorage.RemoveElement(p);
  return true;
}

mozilla::ipc::IPCResult GMPParent::RecvPGMPStorageConstructor(
    PGMPStorageParent* aActor) {
  GMPStorageParent* p = (GMPStorageParent*)aActor;
  if (NS_WARN_IF(NS_FAILED(p->Init()))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPParent::RecvPGMPTimerConstructor(
    PGMPTimerParent* actor) {
  return IPC_OK();
}

PGMPTimerParent* GMPParent::AllocPGMPTimerParent() {
  nsCOMPtr<nsISerialEventTarget> target = GMPEventTarget();
  GMPTimerParent* p = new GMPTimerParent(target);
  mTimers.AppendElement(
      p);  // Released in DeallocPGMPTimerParent, or on shutdown.
  return p;
}

bool GMPParent::DeallocPGMPTimerParent(PGMPTimerParent* aActor) {
  GMPTimerParent* p = static_cast<GMPTimerParent*>(aActor);
  p->Shutdown();
  mTimers.RemoveElement(p);
  return true;
}

bool ReadInfoField(GMPInfoFileParser& aParser, const nsCString& aKey,
                   nsACString& aOutValue) {
  if (!aParser.Contains(aKey) || aParser.Get(aKey).IsEmpty()) {
    return false;
  }
  aOutValue = aParser.Get(aKey);
  return true;
}

RefPtr<GenericPromise> GMPParent::ReadGMPMetaData() {
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(!mName.IsEmpty(), "Plugin mName cannot be empty!");

  nsCOMPtr<nsIFile> infoFile;
  nsresult rv = mDirectory->Clone(getter_AddRefs(infoFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  infoFile->AppendRelativePath(mName + NS_LITERAL_STRING(".info"));

  if (FileExists(infoFile)) {
    return ReadGMPInfoFile(infoFile);
  }

  // Maybe this is the Widevine adapted plugin?
  nsCOMPtr<nsIFile> manifestFile;
  rv = mDirectory->Clone(getter_AddRefs(manifestFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }
  manifestFile->AppendRelativePath(NS_LITERAL_STRING("manifest.json"));
  return ReadChromiumManifestFile(manifestFile);
}

RefPtr<GenericPromise> GMPParent::ReadGMPInfoFile(nsIFile* aFile) {
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
        const nsDependentCSubstring ts(
            Substring(api, tagsStart + 1, tagsEnd - tagsStart - 1));
        nsTArray<nsCString> tagTokens;
        SplitAt(":", ts, tagTokens);
        for (nsCString tag : tagTokens) {
          cap.mAPITags.AppendElement(tag);
        }
      }
    }

    mCapabilities.AppendElement(std::move(cap));
  }

  if (mCapabilities.IsEmpty()) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

RefPtr<GenericPromise> GMPParent::ReadChromiumManifestFile(nsIFile* aFile) {
  nsAutoCString json;
  if (!ReadIntoString(aFile, json, 5 * 1024)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // DOM JSON parsing needs to run on the main thread.
  return InvokeAsync(mMainThread, this, __func__,
                     &GMPParent::ParseChromiumManifest,
                     NS_ConvertUTF8toUTF16(json));
}

static bool IsCDMAPISupported(
    const mozilla::dom::WidevineCDMManifest& aManifest) {
  nsresult ignored;  // Note: ToInteger returns 0 on failure.
  int32_t moduleVersion = aManifest.mX_cdm_module_versions.ToInteger(&ignored);
  int32_t interfaceVersion =
      aManifest.mX_cdm_interface_versions.ToInteger(&ignored);
  int32_t hostVersion = aManifest.mX_cdm_host_versions.ToInteger(&ignored);
  return ChromiumCDMAdapter::Supports(moduleVersion, interfaceVersion,
                                      hostVersion);
}

RefPtr<GenericPromise> GMPParent::ParseChromiumManifest(
    const nsAString& aJSON) {
  LOGD("%s: for '%s'", __FUNCTION__, NS_LossyConvertUTF16toASCII(aJSON).get());

  MOZ_ASSERT(NS_IsMainThread());
  mozilla::dom::WidevineCDMManifest m;
  if (!m.Init(aJSON)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (!IsCDMAPISupported(m)) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mDisplayName = NS_ConvertUTF16toUTF8(m.mName);
  mDescription = NS_ConvertUTF16toUTF8(m.mDescription);
  mVersion = NS_ConvertUTF16toUTF8(m.mVersion);

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  if (!mozilla::SandboxInfo::Get().CanSandboxMedia()) {
    nsPrintfCString msg(
        "GMPParent::ParseChromiumManifest: Plugin \"%s\" is an EME CDM"
        " but this system can't sandbox it; not loading.",
        mDisplayName.get());
    printf_stderr("%s\n", msg.get());
    LOGD("%s", msg.get());
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
#endif

  nsCString kEMEKeySystem;

  // We hard code a few of the settings because they can't be stored in the
  // widevine manifest without making our API different to widevine's.
  if (mDisplayName.EqualsASCII("clearkey")) {
    kEMEKeySystem.AssignLiteral(EME_KEY_SYSTEM_CLEARKEY);
#if XP_WIN
    mLibs = NS_LITERAL_CSTRING(
        "dxva2.dll, msmpeg2vdec.dll, evr.dll, mfh264dec.dll, mfplat.dll");
#endif
  } else if (mDisplayName.EqualsASCII("WidevineCdm")) {
    kEMEKeySystem.AssignLiteral(EME_KEY_SYSTEM_WIDEVINE);
#if XP_WIN
    // psapi.dll added for GetMappedFileNameW, which could possibly be avoided
    // in future versions, see bug 1383611 for details.
    mLibs = NS_LITERAL_CSTRING("dxva2.dll, psapi.dll");
#endif
  } else if (mDisplayName.EqualsASCII("fake")) {
    kEMEKeySystem.AssignLiteral("fake");
#if XP_WIN
    mLibs = NS_LITERAL_CSTRING("dxva2.dll");
#endif
  } else {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  GMPCapability video;

  nsCString codecsString = NS_ConvertUTF16toUTF8(m.mX_cdm_codecs);
  nsTArray<nsCString> codecs;
  SplitAt(",", codecsString, codecs);

  for (const nsCString& chromiumCodec : codecs) {
    nsCString codec;
    if (chromiumCodec.EqualsASCII("vp8")) {
      codec = NS_LITERAL_CSTRING("vp8");
    } else if (chromiumCodec.EqualsASCII("vp9.0")) {
      codec = NS_LITERAL_CSTRING("vp9");
    } else if (chromiumCodec.EqualsASCII("avc1")) {
      codec = NS_LITERAL_CSTRING("h264");
    } else {
      return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }

    video.mAPITags.AppendElement(codec);
  }

  video.mAPITags.AppendElement(kEMEKeySystem);

  video.mAPIName = NS_LITERAL_CSTRING(CHROMIUM_CDM_API);
  mAdapter = NS_LITERAL_STRING("chromium");

  mCapabilities.AppendElement(std::move(video));

  return GenericPromise::CreateAndResolve(true, __func__);
}

bool GMPParent::CanBeSharedCrossNodeIds() const {
  return mNodeId.IsEmpty() &&
         // XXX bug 1159300 hack -- maybe remove after openh264 1.4
         // We don't want to use CDM decoders for non-encrypted playback
         // just yet; especially not for WebRTC. Don't allow CDMs to be used
         // without a node ID.
         !mCanDecrypt;
}

bool GMPParent::CanBeUsedFrom(const nsACString& aNodeId) const {
  return mNodeId == aNodeId;
}

void GMPParent::SetNodeId(const nsACString& aNodeId) {
  MOZ_ASSERT(!aNodeId.IsEmpty());
  mNodeId = aNodeId;
}

const nsCString& GMPParent::GetDisplayName() const { return mDisplayName; }

const nsCString& GMPParent::GetVersion() const { return mVersion; }

uint32_t GMPParent::GetPluginId() const { return mPluginId; }

void GMPParent::ResolveGetContentParentPromises() {
  nsTArray<UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>>> promises;
  promises.SwapElements(mGetContentParentPromises);
  MOZ_ASSERT(mGetContentParentPromises.IsEmpty());
  RefPtr<GMPContentParent::CloseBlocker> blocker(
      new GMPContentParent::CloseBlocker(mGMPContentParent));
  for (auto& holder : promises) {
    holder->Resolve(blocker, __func__);
  }
}

bool GMPParent::OpenPGMPContent() {
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(!mGMPContentParent);

  Endpoint<PGMPContentParent> parent;
  Endpoint<PGMPContentChild> child;
  if (NS_WARN_IF(NS_FAILED(PGMPContent::CreateEndpoints(
          base::GetCurrentProcId(), OtherPid(), &parent, &child)))) {
    return false;
  }

  mGMPContentParent = new GMPContentParent(this);

  if (!parent.Bind(mGMPContentParent)) {
    return false;
  }

  if (!SendInitGMPContentChild(std::move(child))) {
    return false;
  }

  ResolveGetContentParentPromises();

  return true;
}

void GMPParent::RejectGetContentParentPromises() {
  nsTArray<UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>>> promises;
  promises.SwapElements(mGetContentParentPromises);
  MOZ_ASSERT(mGetContentParentPromises.IsEmpty());
  for (auto& holder : promises) {
    holder->Reject(NS_ERROR_FAILURE, __func__);
  }
}

void GMPParent::GetGMPContentParent(
    UniquePtr<MozPromiseHolder<GetGMPContentParentPromise>>&& aPromiseHolder) {
  LOGD("%s %p", __FUNCTION__, this);
  MOZ_ASSERT(GMPEventTarget()->IsOnCurrentThread());

  if (mGMPContentParent) {
    RefPtr<GMPContentParent::CloseBlocker> blocker(
        new GMPContentParent::CloseBlocker(mGMPContentParent));
    aPromiseHolder->Resolve(blocker, __func__);
  } else {
    mGetContentParentPromises.AppendElement(std::move(aPromiseHolder));
    // If we don't have a GMPContentParent and we try to get one for the first
    // time (mGetContentParentPromises.Length() == 1) then call
    // PGMPContent::Open. If more calls to GetGMPContentParent happen before
    // mGMPContentParent has been set then we should just store them, so that
    // they get called when we set mGMPContentParent as a result of the
    // PGMPContent::Open call.
    if (mGetContentParentPromises.Length() == 1) {
      if (!EnsureProcessLoaded() || !OpenPGMPContent()) {
        RejectGetContentParentPromises();
        return;
      }
      // We want to increment this as soon as possible, to avoid that we'd try
      // to shut down the GMP process while we're still trying to get a
      // PGMPContentParent actor.
      ++mGMPContentChildCount;
    }
  }
}

already_AddRefed<GMPContentParent> GMPParent::ForgetGMPContentParent() {
  MOZ_ASSERT(mGetContentParentPromises.IsEmpty());
  return mGMPContentParent.forget();
}

bool GMPParent::EnsureProcessLoaded(base::ProcessId* aID) {
  if (!EnsureProcessLoaded()) {
    return false;
  }
  *aID = OtherPid();
  return true;
}

void GMPParent::IncrementGMPContentChildCount() { ++mGMPContentChildCount; }

nsString GMPParent::GetPluginBaseName() const {
  return NS_LITERAL_STRING("gmp-") + mName;
}

}  // namespace gmp
}  // namespace mozilla

#undef LOG
#undef LOGD

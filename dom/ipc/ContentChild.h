/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Atomics.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/ProcessActor.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nscore.h"
#include "nsHashKeys.h"
#include "nsIDOMProcessChild.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTHashtable.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "nsIFile.h"
#endif

struct ChromePackage;
class nsIObserver;
struct SubstitutionMapping;
struct OverrideMapping;
class nsIDomainPolicy;
class nsIURIClassifierCallback;
class nsDocShellLoadState;
class nsFrameLoader;
class nsIOpenWindowInfo;

namespace mozilla {
class RemoteSpellcheckEngineChild;
class ChildProfilerController;
class BenchmarkStorageChild;

namespace ipc {
class PChildToParentStreamChild;
class PFileDescriptorSetChild;
}  // namespace ipc

namespace loader {
class PScriptCacheChild;
}

namespace widget {
enum class ThemeChangeKind : uint8_t;
}

using mozilla::loader::PScriptCacheChild;

#if !defined(XP_WIN)
// Returns whether or not the currently running build is an unpackaged
// developer build. This check is implemented by looking for omni.ja in the
// the obj/dist dir. We use this routine to detect when the build dir will
// use symlinks to the repo and object dir. On Windows, dev builds don't
// use symlinks.
bool IsDevelopmentBuild();
#endif /* !XP_WIN */

namespace dom {

namespace ipc {
class SharedMap;
}

class AlertObserver;
class ConsoleListener;
class ClonedMessageData;
class BrowserChild;
class TabContext;
enum class CallerType : uint32_t;

class ContentChild final : public PContentChild,
                           public nsIDOMProcessChild,
                           public mozilla::ipc::IShmemAllocator,
                           public mozilla::ipc::ChildToParentStreamActorManager,
                           public ProcessActor {
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::ipc::FileDescriptor FileDescriptor;
  typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;

  friend class PContentChild;

 public:
  NS_DECL_NSIDOMPROCESSCHILD

  ContentChild();
  virtual ~ContentChild();
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override { return 1; }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override { return 1; }

  struct AppInfo {
    nsCString version;
    nsCString buildID;
    nsCString name;
    nsCString UAName;
    nsCString ID;
    nsCString vendor;
    nsCString sourceURL;
    nsCString updateURL;
  };

  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult ProvideWindowCommon(
      BrowserChild* aTabOpener, nsIOpenWindowInfo* aOpenWindowInfo,
      uint32_t aChromeFlags, bool aCalledFromJS, bool aWidthSpecified,
      nsIURI* aURI, const nsAString& aName, const nsACString& aFeatures,
      bool aForceNoOpener, bool aForceNoReferrer,
      nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
      BrowsingContext** aReturn);

  bool Init(MessageLoop* aIOLoop, base::ProcessId aParentPid,
            const char* aParentBuildID, UniquePtr<IPC::Channel> aChannel,
            uint64_t aChildID, bool aIsForBrowser);

  void InitXPCOM(XPCOMInitData&& aXPCOMInit,
                 const mozilla::dom::ipc::StructuredCloneData& aInitialData);

  void InitSharedUASheets(const Maybe<base::SharedMemoryHandle>& aHandle,
                          uintptr_t aAddress);

  void InitGraphicsDeviceData(const ContentDeviceData& aData);

  static ContentChild* GetSingleton() { return sSingleton; }

  const AppInfo& GetAppInfo() { return mAppInfo; }

  void SetProcessName(const nsACString& aName,
                      const nsACString* aETLDplus1 = nullptr);

  void GetProcessName(nsACString& aName) const;

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  void GetProfileDir(nsIFile** aProfileDir) const {
    *aProfileDir = mProfileDir;
    NS_IF_ADDREF(*aProfileDir);
  }

  void SetProfileDir(nsIFile* aProfileDir) { mProfileDir = aProfileDir; }
#endif

  bool IsAlive() const;

  bool IsShuttingDown() const;

  ipc::SharedMap* SharedData() { return mSharedData; };

  static void AppendProcessId(nsACString& aName);

  static void UpdateCookieStatus(nsIChannel* aChannel);

  mozilla::ipc::IPCResult RecvInitGMPService(
      Endpoint<PGMPServiceChild>&& aGMPService);

  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<PProfilerChild>&& aEndpoint);

  mozilla::ipc::IPCResult RecvGMPsChanged(
      nsTArray<GMPCapabilityData>&& capabilities);

  mozilla::ipc::IPCResult RecvInitProcessHangMonitor(
      Endpoint<PProcessHangMonitorChild>&& aHangMonitor);

  mozilla::ipc::IPCResult RecvInitRendering(
      Endpoint<PCompositorManagerChild>&& aCompositor,
      Endpoint<PImageBridgeChild>&& aImageBridge,
      Endpoint<PVRManagerChild>&& aVRBridge,
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager,
      nsTArray<uint32_t>&& namespaces);

  mozilla::ipc::IPCResult RecvRequestPerformanceMetrics(const nsID& aID);

  mozilla::ipc::IPCResult RecvReinitRendering(
      Endpoint<PCompositorManagerChild>&& aCompositor,
      Endpoint<PImageBridgeChild>&& aImageBridge,
      Endpoint<PVRManagerChild>&& aVRBridge,
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager,
      nsTArray<uint32_t>&& namespaces);

  mozilla::ipc::IPCResult RecvReinitRenderingForDeviceReset();

  mozilla::ipc::IPCResult RecvSetProcessSandbox(
      const Maybe<FileDescriptor>& aBroker);

  already_AddRefed<PRemoteLazyInputStreamChild>
  AllocPRemoteLazyInputStreamChild(const nsID& aID, const uint64_t& aSize);

  PHalChild* AllocPHalChild();
  bool DeallocPHalChild(PHalChild*);

  PHeapSnapshotTempFileHelperChild* AllocPHeapSnapshotTempFileHelperChild();

  bool DeallocPHeapSnapshotTempFileHelperChild(
      PHeapSnapshotTempFileHelperChild*);

  PCycleCollectWithLogsChild* AllocPCycleCollectWithLogsChild(
      const bool& aDumpAllTraces, const FileDescriptor& aGCLog,
      const FileDescriptor& aCCLog);

  bool DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* aActor);

  virtual mozilla::ipc::IPCResult RecvPCycleCollectWithLogsConstructor(
      PCycleCollectWithLogsChild* aChild, const bool& aDumpAllTraces,
      const FileDescriptor& aGCLog, const FileDescriptor& aCCLog) override;

  PWebBrowserPersistDocumentChild* AllocPWebBrowserPersistDocumentChild(
      PBrowserChild* aBrowser, const MaybeDiscarded<BrowsingContext>& aContext);

  virtual mozilla::ipc::IPCResult RecvPWebBrowserPersistDocumentConstructor(
      PWebBrowserPersistDocumentChild* aActor, PBrowserChild* aBrowser,
      const MaybeDiscarded<BrowsingContext>& aContext) override;

  bool DeallocPWebBrowserPersistDocumentChild(
      PWebBrowserPersistDocumentChild* aActor);

  PTestShellChild* AllocPTestShellChild();

  bool DeallocPTestShellChild(PTestShellChild*);

  virtual mozilla::ipc::IPCResult RecvPTestShellConstructor(
      PTestShellChild*) override;

  PScriptCacheChild* AllocPScriptCacheChild(const FileDescOrError& cacheFile,
                                            const bool& wantCacheData);

  bool DeallocPScriptCacheChild(PScriptCacheChild*);

  virtual mozilla::ipc::IPCResult RecvPScriptCacheConstructor(
      PScriptCacheChild*, const FileDescOrError& cacheFile,
      const bool& wantCacheData) override;

  PNeckoChild* AllocPNeckoChild();

  bool DeallocPNeckoChild(PNeckoChild*);

  PPrintingChild* AllocPPrintingChild();

  bool DeallocPPrintingChild(PPrintingChild*);

  PChildToParentStreamChild* AllocPChildToParentStreamChild();
  bool DeallocPChildToParentStreamChild(PChildToParentStreamChild*);

  PParentToChildStreamChild* AllocPParentToChildStreamChild();
  bool DeallocPParentToChildStreamChild(PParentToChildStreamChild*);

  PMediaChild* AllocPMediaChild();

  bool DeallocPMediaChild(PMediaChild* aActor);

  PBenchmarkStorageChild* AllocPBenchmarkStorageChild();

  bool DeallocPBenchmarkStorageChild(PBenchmarkStorageChild* aActor);

  PPresentationChild* AllocPPresentationChild();

  bool DeallocPPresentationChild(PPresentationChild* aActor);

  mozilla::ipc::IPCResult RecvNotifyPresentationReceiverLaunched(
      PBrowserChild* aIframe, const nsString& aSessionId);

  mozilla::ipc::IPCResult RecvNotifyPresentationReceiverCleanUp(
      const nsString& aSessionId);

  mozilla::ipc::IPCResult RecvNotifyEmptyHTTPCache();

#ifdef MOZ_WEBSPEECH
  PSpeechSynthesisChild* AllocPSpeechSynthesisChild();
  bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor);
#endif

  mozilla::ipc::IPCResult RecvRegisterChrome(
      nsTArray<ChromePackage>&& packages,
      nsTArray<SubstitutionMapping>&& resources,
      nsTArray<OverrideMapping>&& overrides, const nsCString& locale,
      const bool& reset);
  mozilla::ipc::IPCResult RecvRegisterChromeItem(
      const ChromeRegistryItem& item);

  mozilla::ipc::IPCResult RecvClearStyleSheetCache(
      const Maybe<RefPtr<nsIPrincipal>>& aForPrincipal);
  mozilla::ipc::IPCResult RecvClearImageCache(const bool& privateLoader,
                                              const bool& chrome);

  PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild();

  bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*);

  mozilla::ipc::IPCResult RecvSetOffline(const bool& offline);

  mozilla::ipc::IPCResult RecvSetConnectivity(const bool& connectivity);
  mozilla::ipc::IPCResult RecvSetCaptivePortalState(const int32_t& state);

  mozilla::ipc::IPCResult RecvBidiKeyboardNotify(const bool& isLangRTL,
                                                 const bool& haveBidiKeyboards);

  mozilla::ipc::IPCResult RecvNotifyVisited(nsTArray<VisitedQueryResult>&&);

  mozilla::ipc::IPCResult RecvThemeChanged(LookAndFeelData&& aLookAndFeelData,
                                           widget::ThemeChangeKind);

  mozilla::ipc::IPCResult RecvUpdateSystemParameters(
      nsTArray<SystemParameterKVPair>&& aUpdates);

  // auto remove when alertfinished is received.
  nsresult AddRemoteAlertObserver(const nsString& aData,
                                  nsIObserver* aObserver);

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref);
  mozilla::ipc::IPCResult RecvVarUpdate(const GfxVarUpdate& pref);

  mozilla::ipc::IPCResult RecvUpdatePerfStatsCollectionMask(
      const uint64_t& aMask);

  mozilla::ipc::IPCResult RecvCollectPerfStatsJSON(
      CollectPerfStatsJSONResolver&& aResolver);

  mozilla::ipc::IPCResult RecvDataStoragePut(const nsString& aFilename,
                                             const DataStorageItem& aItem);

  mozilla::ipc::IPCResult RecvDataStorageRemove(const nsString& aFilename,
                                                const nsCString& aKey,
                                                const DataStorageType& aType);

  mozilla::ipc::IPCResult RecvDataStorageClear(const nsString& aFilename);

  mozilla::ipc::IPCResult RecvNotifyAlertsObserver(const nsCString& aType,
                                                   const nsString& aData);

  mozilla::ipc::IPCResult RecvLoadProcessScript(const nsString& aURL);

  mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                           const ClonedMessageData& aData);

  mozilla::ipc::IPCResult RecvRegisterStringBundles(
      nsTArray<StringBundleDescriptor>&& stringBundles);

  mozilla::ipc::IPCResult RecvUpdateSharedData(
      const FileDescriptor& aMapFile, const uint32_t& aMapSize,
      nsTArray<IPCBlob>&& aBlobs, nsTArray<nsCString>&& aChangedKeys);

  mozilla::ipc::IPCResult RecvFontListChanged();

  mozilla::ipc::IPCResult RecvGeolocationUpdate(nsIDOMGeoPosition* aPosition);

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we don't have MOZ_CAN_RUN_SCRIPT bits
  // in IPC code yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvGeolocationError(const uint16_t& errorCode);

  mozilla::ipc::IPCResult RecvUpdateDictionaryList(
      nsTArray<nsCString>&& aDictionaries);

  mozilla::ipc::IPCResult RecvUpdateFontList(
      nsTArray<SystemFontListEntry>&& aFontList);
  mozilla::ipc::IPCResult RecvRebuildFontList(const bool& aFullRebuild);

  mozilla::ipc::IPCResult RecvUpdateAppLocales(
      nsTArray<nsCString>&& aAppLocales);
  mozilla::ipc::IPCResult RecvUpdateRequestedLocales(
      nsTArray<nsCString>&& aRequestedLocales);

  mozilla::ipc::IPCResult RecvAddPermission(const IPC::Permission& permission);

  mozilla::ipc::IPCResult RecvRemoveAllPermissions();

  mozilla::ipc::IPCResult RecvFlushMemory(const nsString& reason);

  mozilla::ipc::IPCResult RecvActivateA11y(const uint32_t& aMainChromeTid,
                                           const uint32_t& aMsaaID);
  mozilla::ipc::IPCResult RecvShutdownA11y();

  mozilla::ipc::IPCResult RecvApplicationForeground();
  mozilla::ipc::IPCResult RecvApplicationBackground();
  mozilla::ipc::IPCResult RecvGarbageCollect();
  mozilla::ipc::IPCResult RecvCycleCollect();
  mozilla::ipc::IPCResult RecvUnlinkGhosts();

  mozilla::ipc::IPCResult RecvAppInfo(
      const nsCString& version, const nsCString& buildID, const nsCString& name,
      const nsCString& UAName, const nsCString& ID, const nsCString& vendor,
      const nsCString& sourceURL, const nsCString& updateURL);

  mozilla::ipc::IPCResult RecvRemoteType(const nsCString& aRemoteType);

  // Call RemoteTypePrefix() on the result to remove URIs if you want to use
  // this for telemetry.
  const nsACString& GetRemoteType() const override;

  mozilla::ipc::IPCResult RecvInitServiceWorkers(
      const ServiceWorkerConfiguration& aConfig);

  mozilla::ipc::IPCResult RecvInitBlobURLs(
      nsTArray<BlobURLRegistrationData>&& aRegistations);

  mozilla::ipc::IPCResult RecvInitJSActorInfos(
      nsTArray<JSProcessActorInfo>&& aContentInfos,
      nsTArray<JSWindowActorInfo>&& aWindowInfos);

  mozilla::ipc::IPCResult RecvUnregisterJSWindowActor(const nsCString& aName);

  mozilla::ipc::IPCResult RecvUnregisterJSProcessActor(const nsCString& aName);

  mozilla::ipc::IPCResult RecvLastPrivateDocShellDestroyed();

  mozilla::ipc::IPCResult RecvNotifyProcessPriorityChanged(
      const hal::ProcessPriority& aPriority);

  mozilla::ipc::IPCResult RecvMinimizeMemoryUsage();

  mozilla::ipc::IPCResult RecvLoadAndRegisterSheet(nsIURI* aURI,
                                                   const uint32_t& aType);

  mozilla::ipc::IPCResult RecvUnregisterSheet(nsIURI* aURI,
                                              const uint32_t& aType);

  void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  mozilla::ipc::IPCResult RecvNotifyIdleObserver(const uint64_t& aObserver,
                                                 const nsCString& aTopic,
                                                 const nsString& aData);

  mozilla::ipc::IPCResult RecvUpdateWindow(const uintptr_t& aChildId);

  mozilla::ipc::IPCResult RecvDomainSetChanged(const uint32_t& aSetType,
                                               const uint32_t& aChangeType,
                                               nsIURI* aDomain);

  mozilla::ipc::IPCResult RecvShutdown();

  mozilla::ipc::IPCResult RecvInvokeDragSession(
      nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvEndDragSession(
      const bool& aDoneDrag, const bool& aUserCancelled,
      const mozilla::LayoutDeviceIntPoint& aEndDragPoint,
      const uint32_t& aKeyModifiers);

  mozilla::ipc::IPCResult RecvPush(const nsCString& aScope,
                                   const IPC::Principal& aPrincipal,
                                   const nsString& aMessageId);

  mozilla::ipc::IPCResult RecvPushWithData(const nsCString& aScope,
                                           const IPC::Principal& aPrincipal,
                                           const nsString& aMessageId,
                                           nsTArray<uint8_t>&& aData);

  mozilla::ipc::IPCResult RecvPushSubscriptionChange(
      const nsCString& aScope, const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvPushError(const nsCString& aScope,
                                        const IPC::Principal& aPrincipal,
                                        const nsString& aMessage,
                                        const uint32_t& aFlags);

  mozilla::ipc::IPCResult RecvNotifyPushSubscriptionModifiedObservers(
      const nsCString& aScope, const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvRefreshScreens(
      nsTArray<ScreenDetails>&& aScreens);

  mozilla::ipc::IPCResult RecvNetworkLinkTypeChange(const uint32_t& aType);
  uint32_t NetworkLinkType() const { return mNetworkLinkType; }

  // Get the directory for IndexedDB files. We query the parent for this and
  // cache the value
  nsString& GetIndexedDBPath();

  ContentParentId GetID() const { return mID; }

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  uint32_t GetChromeMainThreadId() const { return mMainChromeTid; }

  uint32_t GetMsaaID() const { return mMsaaID; }
#endif

  bool IsForBrowser() const { return mIsForBrowser; }

  PFileDescriptorSetChild* AllocPFileDescriptorSetChild(const FileDescriptor&);

  bool DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY mozilla::ipc::IPCResult RecvConstructBrowser(
      ManagedEndpoint<PBrowserChild>&& aBrowserEp,
      ManagedEndpoint<PWindowGlobalChild>&& aWindowEp, const TabId& aTabId,
      const IPCTabContext& aContext, const WindowGlobalInit& aWindowInit,
      const uint32_t& aChromeFlags, const ContentParentId& aCpID,
      const bool& aIsForBrowser, const bool& aIsTopLevel);

  FORWARD_SHMEM_ALLOCATOR_TO(PContentChild)

  void GetAvailableDictionaries(nsTArray<nsCString>& aDictionaries);

  PWebrtcGlobalChild* AllocPWebrtcGlobalChild();

  bool DeallocPWebrtcGlobalChild(PWebrtcGlobalChild* aActor);

  PContentPermissionRequestChild* AllocPContentPermissionRequestChild(
      const nsTArray<PermissionRequest>& aRequests,
      const IPC::Principal& aPrincipal,
      const IPC::Principal& aTopLevelPrincipal,
      const bool& aIsHandlingUserInput,
      const bool& aMaybeUnsafePermissionDelegate, const TabId& aTabId);
  bool DeallocPContentPermissionRequestChild(
      PContentPermissionRequestChild* actor);

  // GetFiles for WebKit/Blink FileSystem API and Directory API must run on the
  // parent process.
  void CreateGetFilesRequest(const nsAString& aDirectoryPath,
                             bool aRecursiveFlag, nsID& aUUID,
                             GetFilesHelperChild* aChild);

  void DeleteGetFilesRequest(nsID& aUUID, GetFilesHelperChild* aChild);

  mozilla::ipc::IPCResult RecvGetFilesResponse(
      const nsID& aUUID, const GetFilesResponseResult& aResult);

  mozilla::ipc::IPCResult RecvBlobURLRegistration(
      const nsCString& aURI, const IPCBlob& aBlob,
      const IPC::Principal& aPrincipal, const Maybe<nsID>& aAgentClusterId);

  mozilla::ipc::IPCResult RecvBlobURLUnregistration(const nsCString& aURI);

  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage, const Maybe<FileDescriptor>& DMDFile,
      const RequestMemoryReportResolver& aResolver);

  mozilla::ipc::IPCResult RecvGetUntrustedModulesData(
      GetUntrustedModulesDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvSetXPCOMProcessAttributes(
      XPCOMInitData&& aXPCOMInit, const StructuredCloneData& aInitialData,
      LookAndFeelData&& aLookAndFeelData,
      nsTArray<SystemFontListEntry>&& aFontList,
      const Maybe<base::SharedMemoryHandle>& aSharedUASheetHandle,
      const uintptr_t& aSharedUASheetAddress,
      nsTArray<base::SharedMemoryHandle>&& aSharedFontListBlocks);

  mozilla::ipc::IPCResult RecvProvideAnonymousTemporaryFile(
      const uint64_t& aID, const FileDescOrError& aFD);

  mozilla::ipc::IPCResult RecvSetPermissionsWithKey(
      const nsCString& aPermissionKey, nsTArray<IPC::Permission>&& aPerms);

  mozilla::ipc::IPCResult RecvShareCodeCoverageMutex(
      const CrossProcessMutexHandle& aHandle);

  mozilla::ipc::IPCResult RecvFlushCodeCoverageCounters(
      FlushCodeCoverageCountersResolver&& aResolver);

  mozilla::ipc::IPCResult RecvSetInputEventQueueEnabled();

  mozilla::ipc::IPCResult RecvFlushInputEventQueue();

  mozilla::ipc::IPCResult RecvSuspendInputEventQueue();

  mozilla::ipc::IPCResult RecvResumeInputEventQueue();

  mozilla::ipc::IPCResult RecvAddDynamicScalars(
      nsTArray<DynamicScalarDefinition>&& aDefs);

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  bool SendGetA11yContentId();
#endif  // defined(XP_WIN) && defined(ACCESSIBILITY)

  // Get a reference to the font list passed from the chrome process,
  // for use during gfx initialization.
  nsTArray<mozilla::dom::SystemFontListEntry>& SystemFontList() {
    return mFontList;
  }

  nsTArray<base::SharedMemoryHandle>& SharedFontListBlocks() {
    return mSharedFontListBlocks;
  }

  // PURLClassifierChild
  PURLClassifierChild* AllocPURLClassifierChild(const Principal& aPrincipal,
                                                bool* aSuccess);
  bool DeallocPURLClassifierChild(PURLClassifierChild* aActor);

  // PURLClassifierLocalChild
  PURLClassifierLocalChild* AllocPURLClassifierLocalChild(
      nsIURI* aUri, const nsTArray<IPCURLClassifierFeature>& aFeatures);
  bool DeallocPURLClassifierLocalChild(PURLClassifierLocalChild* aActor);

  PLoginReputationChild* AllocPLoginReputationChild(nsIURI* aUri);

  bool DeallocPLoginReputationChild(PLoginReputationChild* aActor);

  PSessionStorageObserverChild* AllocPSessionStorageObserverChild();

  bool DeallocPSessionStorageObserverChild(
      PSessionStorageObserverChild* aActor);

  LookAndFeelData& BorrowLookAndFeelData() { return mLookAndFeelData; }

  /**
   * Helper function for protocols that use the GPU process when available.
   * Overrides FatalError to just be a warning when communicating with the
   * GPU process since we don't want to crash the content process when the
   * GPU process crashes.
   */
  static void FatalErrorIfNotUsingGPUProcess(const char* const aErrorMsg,
                                             base::ProcessId aOtherPid);

  typedef std::function<void(PRFileDesc*)> AnonymousTemporaryFileCallback;
  nsresult AsyncOpenAnonymousTemporaryFile(
      const AnonymousTemporaryFileCallback& aCallback);

  mozilla::ipc::IPCResult RecvSetPluginList(
      const uint32_t& aPluginEpoch, nsTArray<PluginTag>&& aPluginTags,
      nsTArray<FakePluginTag>&& aFakePluginTags);

  mozilla::ipc::IPCResult RecvSaveRecording(const FileDescriptor& aFile);

  mozilla::ipc::IPCResult RecvCrossProcessRedirect(
      RedirectToRealChannelArgs&& aArgs,
      nsTArray<Endpoint<extensions::PStreamFilterParent>>&& aEndpoints,
      CrossProcessRedirectResolver&& aResolve);

  mozilla::ipc::IPCResult RecvStartDelayedAutoplayMediaComponents(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvUpdateMediaControlAction(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const MediaControlAction& aAction);

  void HoldBrowsingContextGroup(BrowsingContextGroup* aBCG);
  void ReleaseBrowsingContextGroup(BrowsingContextGroup* aBCG);

  // See `BrowsingContext::mEpochs` for an explanation of this field.
  uint64_t GetBrowsingContextFieldEpoch() const {
    return mBrowsingContextFieldEpoch;
  }
  uint64_t NextBrowsingContextFieldEpoch() {
    mBrowsingContextFieldEpoch++;
    return mBrowsingContextFieldEpoch;
  }

  mozilla::ipc::IPCResult RecvOnAllowAccessFor(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const nsCString& aTrackingOrigin, uint32_t aCookieBehavior,
      const ContentBlockingNotifier::StorageAccessPermissionGrantedReason&
          aReason);

  mozilla::ipc::IPCResult RecvOnContentBlockingDecision(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const ContentBlockingNotifier::BlockingDecision& aDecision,
      uint32_t aRejectedReason);

#ifdef NIGHTLY_BUILD
  // Fetch the current number of pending input events.
  //
  // NOTE: This method performs an atomic read, and is safe to call from all
  // threads.
  uint32_t GetPendingInputEvents() { return mPendingInputEvents; }
#endif

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
  mozilla::ipc::IPCResult RecvInitSandboxTesting(
      Endpoint<PSandboxTestingChild>&& aEndpoint);
#endif

  PChildToParentStreamChild* SendPChildToParentStreamConstructor(
      PChildToParentStreamChild* aActor) override;
  PFileDescriptorSetChild* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) override;

 private:
  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);
  void StartForceKillTimer();

  void ShutdownInternal();

  mozilla::ipc::IPCResult GetResultForRenderingInitFailure(
      base::ProcessId aOtherPid);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  virtual void OnChannelReceivedMessage(const Message& aMsg) override;

  mozilla::ipc::IPCResult RecvCreateBrowsingContext(
      uint64_t aGroupId, BrowsingContext::IPCInitializer&& aInit);

  mozilla::ipc::IPCResult RecvDiscardBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext,
      DiscardBrowsingContextResolver&& aResolve);

  mozilla::ipc::IPCResult RecvRegisterBrowsingContextGroup(
      uint64_t aGroupId, nsTArray<SyncedContextInitializer>&& aInits);

  mozilla::ipc::IPCResult RecvWindowClose(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aTrustedCaller);
  mozilla::ipc::IPCResult RecvWindowFocus(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvWindowBlur(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType);
  mozilla::ipc::IPCResult RecvRaiseWindow(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvAdjustWindowFocus(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aIsVisible,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvClearFocus(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetFocusedBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvAbortOrientationPendingPromises(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvUnsetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvSetFocusedElement(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aNeedsFocus);
  mozilla::ipc::IPCResult RecvFinalizeFocusOuter(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aCanFocus,
      CallerType aCallerType);
  mozilla::ipc::IPCResult RecvBlurToChild(
      const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
      const MaybeDiscarded<BrowsingContext>& aBrowsingContextToClear,
      const MaybeDiscarded<BrowsingContext>& aAncestorBrowsingContextToFocus,
      bool aIsLeavingDocument, bool aAdjustWidget, uint64_t aActionId);
  mozilla::ipc::IPCResult RecvSetupFocusedAndActive(
      const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
      const MaybeDiscarded<BrowsingContext>& aActiveBrowsingContext,
      uint64_t aActionId);
  mozilla::ipc::IPCResult RecvReviseActiveBrowsingContext(
      uint64_t aOldActionId,
      const MaybeDiscarded<BrowsingContext>& aActiveBrowsingContext,
      uint64_t aNewActionId);
  mozilla::ipc::IPCResult RecvMaybeExitFullscreen(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvWindowPostMessage(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const ClonedOrErrorMessageData& aMessage, const PostMessageData& aData);

  mozilla::ipc::IPCResult RecvCommitBrowsingContextTransaction(
      const MaybeDiscarded<BrowsingContext>& aContext,
      BrowsingContext::BaseTransaction&& aTransaction, uint64_t aEpoch);

  mozilla::ipc::IPCResult RecvCommitWindowContextTransaction(
      const MaybeDiscarded<WindowContext>& aContext,
      WindowContext::BaseTransaction&& aTransaction, uint64_t aEpoch);

  mozilla::ipc::IPCResult RecvCreateWindowContext(
      WindowContext::IPCInitializer&& aInit);
  mozilla::ipc::IPCResult RecvDiscardWindowContext(
      uint64_t aContextId, DiscardWindowContextResolver&& aResolve);

  mozilla::ipc::IPCResult RecvScriptError(
      const nsString& aMessage, const nsString& aSourceName,
      const nsString& aSourceLine, const uint32_t& aLineNumber,
      const uint32_t& aColNumber, const uint32_t& aFlags,
      const nsCString& aCategory, const bool& aFromPrivateWindow,
      const uint64_t& aInnerWindowId, const bool& aFromChromeContext);

  mozilla::ipc::IPCResult RecvReportFrameTimingData(
      uint64_t innerWindowId, const nsString& entryName,
      const nsString& initiatorType, UniquePtr<PerformanceTimingData>&& aData);

  mozilla::ipc::IPCResult RecvLoadURI(
      const MaybeDiscarded<BrowsingContext>& aContext,
      nsDocShellLoadState* aLoadState, bool aSetNavigating,
      LoadURIResolver&& aResolve);

  mozilla::ipc::IPCResult RecvInternalLoad(nsDocShellLoadState* aLoadState);

  mozilla::ipc::IPCResult RecvDisplayLoadError(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsAString& aURI);

  mozilla::ipc::IPCResult RecvGoBack(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const Maybe<int32_t>& aCancelContentJSEpoch,
      bool aRequireUserInteraction);
  mozilla::ipc::IPCResult RecvGoForward(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const Maybe<int32_t>& aCancelContentJSEpoch,
      bool aRequireUserInteraction);
  mozilla::ipc::IPCResult RecvGoToIndex(
      const MaybeDiscarded<BrowsingContext>& aContext, const int32_t& aIndex,
      const Maybe<int32_t>& aCancelContentJSEpoch);
  mozilla::ipc::IPCResult RecvReload(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const uint32_t aReloadFlags);
  mozilla::ipc::IPCResult RecvStopLoad(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const uint32_t aStopFlags);

  mozilla::ipc::IPCResult RecvRawMessage(
      const JSActorMessageMeta& aMeta, const Maybe<ClonedMessageData>& aData,
      const Maybe<ClonedMessageData>& aStack);

  already_AddRefed<JSActor> InitJSActor(JS::HandleObject aMaybeActor,
                                        const nsACString& aName,
                                        ErrorResult& aRv) override;
  mozilla::ipc::IProtocol* AsNativeActor() override { return this; }

  mozilla::ipc::IPCResult RecvHistoryCommitIndexAndLength(
      const MaybeDiscarded<BrowsingContext>& aContext, const uint32_t& aIndex,
      const uint32_t& aLength, const nsID& aChangeID);

  mozilla::ipc::IPCResult RecvDispatchLocationChangeEvent(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvDispatchBeforeUnloadToSubtree(
      const MaybeDiscarded<BrowsingContext>& aStartingAt,
      DispatchBeforeUnloadToSubtreeResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCanSavePresentation(
      const MaybeDiscarded<BrowsingContext>& aTopLevelContext,
      Maybe<uint64_t> aDocumentChannelId,
      CanSavePresentationResolver&& aResolve);

 public:
  static void DispatchBeforeUnloadToSubtree(
      BrowsingContext* aStartingAt,
      const DispatchBeforeUnloadToSubtreeResolver& aResolver);

 private:
  mozilla::ipc::IPCResult RecvFlushFOGData(FlushFOGDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvUpdateMediaCodecsSupported(
      RemoteDecodeIn aLocation,
      const PDMFactory::MediaCodecsSupported& aSupported);

#ifdef NIGHTLY_BUILD
  virtual PContentChild::Result OnMessageReceived(const Message& aMsg) override;
#else
  using PContentChild::OnMessageReceived;
#endif

  virtual PContentChild::Result OnMessageReceived(const Message& aMsg,
                                                  Message*& aReply) override;

  nsTArray<mozilla::UniquePtr<AlertObserver>> mAlertObservers;
  RefPtr<ConsoleListener> mConsoleListener;

  nsTHashtable<nsPtrHashKey<nsIObserver>> mIdleObservers;

  nsTArray<nsCString> mAvailableDictionaries;

  // Temporary storage for a list of available fonts, passed from the
  // parent process and used to initialize gfx in the child. Currently used
  // only on MacOSX and Linux.
  nsTArray<mozilla::dom::SystemFontListEntry> mFontList;
  // Temporary storage for look and feel data.
  LookAndFeelData mLookAndFeelData;
  // Temporary storage for list of shared-fontlist memory blocks.
  nsTArray<base::SharedMemoryHandle> mSharedFontListBlocks;

  /**
   * An ID unique to the process containing our corresponding
   * content parent.
   *
   * We expect our content parent to set this ID immediately after opening a
   * channel to us.
   */
  ContentParentId mID;

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  /**
   * The thread ID of the main thread in the chrome process.
   */
  uint32_t mMainChromeTid;

  /**
   * This is an a11y-specific unique id for the content process that is
   * generated by the chrome process.
   */
  uint32_t mMsaaID;
#endif  // defined(XP_WIN) && defined(ACCESSIBILITY)

  AppInfo mAppInfo;

  bool mIsForBrowser;
  nsCString mRemoteType = NOT_REMOTE_TYPE;
  bool mIsAlive;
  nsCString mProcessName;

  static ContentChild* sSingleton;

  class ShutdownCanary;
  static StaticAutoPtr<ShutdownCanary> sShutdownCanary;

  nsCOMPtr<nsIDomainPolicy> mPolicy;
  nsCOMPtr<nsITimer> mForceKillTimer;

  RefPtr<ipc::SharedMap> mSharedData;

#ifdef MOZ_GECKO_PROFILER
  RefPtr<ChildProfilerController> mProfilerController;
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  nsCOMPtr<nsIFile> mProfileDir;
#endif

  // Hashtable to keep track of the pending GetFilesHelper objects.
  // This GetFilesHelperChild objects are removed when RecvGetFilesResponse is
  // received.
  nsRefPtrHashtable<nsIDHashKey, GetFilesHelperChild> mGetFilesPendingRequests;

  nsClassHashtable<nsUint64HashKey, AnonymousTemporaryFileCallback>
      mPendingAnonymousTemporaryFiles;

  mozilla::Atomic<bool> mShuttingDown;

#ifdef NIGHTLY_BUILD
  // NOTE: This member is atomic because it can be accessed from
  // off-main-thread.
  mozilla::Atomic<uint32_t> mPendingInputEvents;
#endif

  uint32_t mNetworkLinkType = 0;

  // See `BrowsingContext::mEpochs` for an explanation of this field.
  uint64_t mBrowsingContextFieldEpoch = 0;
};

inline nsISupports* ToSupports(mozilla::dom::ContentChild* aContentChild) {
  return static_cast<nsIDOMProcessChild*>(aContentChild);
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ContentChild_h

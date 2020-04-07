/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "base/shared_memory.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/PBrowserOrId.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/CPOWManagerGetter.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsHashKeys.h"
#include "nsIContentChild.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nsRefPtrHashtable.h"

#include "nsIWindowProvider.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "nsIFile.h"
#endif

struct ChromePackage;
class nsIObserver;
struct SubstitutionMapping;
struct OverrideMapping;
class nsIDomainPolicy;
class nsIURIClassifierCallback;
struct LookAndFeelInt;
class nsDocShellLoadState;
class nsFrameLoader;

namespace mozilla {
class RemoteSpellcheckEngineChild;
class ChildProfilerController;
class BenchmarkStorageChild;

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
class GetFilesHelperChild;
class TabContext;
enum class MediaControlKeysEvent : uint32_t;

class ContentChild final
    : public PContentChild,
      public nsIContentChild,
      public nsIWindowProvider,
      public CPOWManagerGetter,
      public mozilla::ipc::IShmemAllocator,
      public mozilla::ipc::ChildToParentStreamActorManager {
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::ipc::FileDescriptor FileDescriptor;
  typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;

  friend class PContentChild;

 public:
  NS_DECL_NSICONTENTCHILD
  NS_DECL_NSIWINDOWPROVIDER

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

  nsresult ProvideWindowCommon(BrowserChild* aTabOpener,
                               mozIDOMWindowProxy* aParent, bool aIframeMoz,
                               uint32_t aChromeFlags, bool aCalledFromJS,
                               bool aWidthSpecified, nsIURI* aURI,
                               const nsAString& aName,
                               const nsACString& aFeatures, bool aForceNoOpener,
                               bool aForceNoReferrer,
                               nsDocShellLoadState* aLoadState,
                               bool* aWindowIsNew, BrowsingContext** aReturn);

  bool Init(MessageLoop* aIOLoop, base::ProcessId aParentPid,
            const char* aParentBuildID, UniquePtr<IPC::Channel> aChannel,
            uint64_t aChildID, bool aIsForBrowser);

  void InitXPCOM(const XPCOMInitData& aXPCOMInit,
                 const mozilla::dom::ipc::StructuredCloneData& aInitialData);

  void InitSharedUASheets(const Maybe<base::SharedMemoryHandle>& aHandle,
                          uintptr_t aAddress);

  void InitGraphicsDeviceData(const ContentDeviceData& aData);

  static ContentChild* GetSingleton() { return sSingleton; }

  const AppInfo& GetAppInfo() { return mAppInfo; }

  void SetProcessName(const nsAString& aName);

  void GetProcessName(nsAString& aName) const;

  void GetProcessName(nsACString& aName) const;

  void LaunchRDDProcess();

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

  mozilla::ipc::IPCResult RecvAudioDefaultDeviceChange();

  mozilla::ipc::IPCResult RecvReinitRenderingForDeviceReset();

  mozilla::ipc::IPCResult RecvSetProcessSandbox(
      const Maybe<FileDescriptor>& aBroker);

  already_AddRefed<PIPCBlobInputStreamChild> AllocPIPCBlobInputStreamChild(
      const nsID& aID, const uint64_t& aSize);

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
      PBrowserChild* aBrowser, const uint64_t& aOuterWindowID);

  virtual mozilla::ipc::IPCResult RecvPWebBrowserPersistDocumentConstructor(
      PWebBrowserPersistDocumentChild* aActor, PBrowserChild* aBrowser,
      const uint64_t& aOuterWindowID) override;

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

  jsipc::CPOWManager* GetCPOWManager() override;

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

  mozilla::ipc::IPCResult RecvClearImageCache(const bool& privateLoader,
                                              const bool& chrome);

  mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild();

  bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*);

  PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild();

  bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*);

  mozilla::ipc::IPCResult RecvSetOffline(const bool& offline);

  mozilla::ipc::IPCResult RecvSetConnectivity(const bool& connectivity);
  mozilla::ipc::IPCResult RecvSetCaptivePortalState(const int32_t& state);

  mozilla::ipc::IPCResult RecvBidiKeyboardNotify(const bool& isLangRTL,
                                                 const bool& haveBidiKeyboards);

  mozilla::ipc::IPCResult RecvNotifyVisited(nsTArray<VisitedQueryResult>&&);

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
                                           nsTArray<CpowEntry>&& aCpows,
                                           const IPC::Principal& aPrincipal,
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
      nsTArray<nsString>&& aDictionaries);

  mozilla::ipc::IPCResult RecvUpdateFontList(
      nsTArray<SystemFontListEntry>&& aFontList);
  mozilla::ipc::IPCResult RecvRebuildFontList();

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

  mozilla::ipc::IPCResult RecvGarbageCollect();
  mozilla::ipc::IPCResult RecvCycleCollect();
  mozilla::ipc::IPCResult RecvUnlinkGhosts();

  mozilla::ipc::IPCResult RecvAppInfo(
      const nsCString& version, const nsCString& buildID, const nsCString& name,
      const nsCString& UAName, const nsCString& ID, const nsCString& vendor,
      const nsCString& sourceURL, const nsCString& updateURL);

  mozilla::ipc::IPCResult RecvRemoteType(const nsString& aRemoteType);

  // Call RemoteTypePrefix() on the result to remove URIs if you want to use
  // this for telemetry.
  const nsAString& GetRemoteType() const;

  mozilla::ipc::IPCResult RecvInitServiceWorkers(
      const ServiceWorkerConfiguration& aConfig);

  mozilla::ipc::IPCResult RecvInitBlobURLs(
      nsTArray<BlobURLRegistrationData>&& aRegistations);

  mozilla::ipc::IPCResult RecvInitJSWindowActorInfos(
      nsTArray<JSWindowActorInfo>&& aInfos);

  mozilla::ipc::IPCResult RecvUnregisterJSWindowActor(const nsString& aName);

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

  mozilla::ipc::IPCResult RecvActivate(PBrowserChild* aTab);

  mozilla::ipc::IPCResult RecvDeactivate(PBrowserChild* aTab);

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

  mozilla::ipc::IPCResult RecvConstructBrowser(
      ManagedEndpoint<PBrowserChild>&& aBrowserEp,
      ManagedEndpoint<PWindowGlobalChild>&& aWindowEp, const TabId& aTabId,
      const TabId& aSameTabGroupAs, const IPCTabContext& aContext,
      const WindowGlobalInit& aWindowInit, const uint32_t& aChromeFlags,
      const ContentParentId& aCpID, const bool& aIsForBrowser,
      const bool& aIsTopLevel);

  FORWARD_SHMEM_ALLOCATOR_TO(PContentChild)

  void GetAvailableDictionaries(nsTArray<nsString>& aDictionaries);

  PBrowserOrId GetBrowserOrId(BrowserChild* aBrowserChild);

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
      const IPC::Principal& aPrincipal);

  mozilla::ipc::IPCResult RecvBlobURLUnregistration(const nsCString& aURI);

  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage, const Maybe<FileDescriptor>& DMDFile);

  mozilla::ipc::IPCResult RecvGetUntrustedModulesData(
      GetUntrustedModulesDataResolver&& aResolver);

  mozilla::ipc::IPCResult RecvSetXPCOMProcessAttributes(
      const XPCOMInitData& aXPCOMInit, const StructuredCloneData& aInitialData,
      nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache,
      nsTArray<SystemFontListEntry>&& aFontList,
      const Maybe<base::SharedMemoryHandle>& aSharedUASheetHandle,
      const uintptr_t& aSharedUASheetAddress);

  mozilla::ipc::IPCResult RecvProvideAnonymousTemporaryFile(
      const uint64_t& aID, const FileDescOrError& aFD);

  mozilla::ipc::IPCResult RecvSetPermissionsWithKey(
      const nsCString& aPermissionKey, nsTArray<IPC::Permission>&& aPerms);

  mozilla::ipc::IPCResult RecvShareCodeCoverageMutex(
      const CrossProcessMutexHandle& aHandle);

  mozilla::ipc::IPCResult RecvFlushCodeCoverageCounters(
      FlushCodeCoverageCountersResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetMemoryUniqueSetSize(
      GetMemoryUniqueSetSizeResolver&& aResolver);

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

  PSHEntryChild* AllocPSHEntryChild(PSHistoryChild* aSHistory,
                                    uint64_t aSharedID);

  void DeallocPSHEntryChild(PSHEntryChild*);

  PSHistoryChild* AllocPSHistoryChild(
      const MaybeDiscarded<BrowsingContext>& aContext);

  void DeallocPSHistoryChild(PSHistoryChild* aActor);

  nsTArray<LookAndFeelInt>& LookAndFeelCache() { return mLookAndFeelCache; }

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

  already_AddRefed<nsIEventTarget> GetEventTargetFor(
      BrowserChild* aBrowserChild);

  mozilla::ipc::IPCResult RecvSetPluginList(
      const uint32_t& aPluginEpoch, nsTArray<PluginTag>&& aPluginTags,
      nsTArray<FakePluginTag>&& aFakePluginTags);

  mozilla::ipc::IPCResult RecvSaveRecording(const FileDescriptor& aFile);

  mozilla::ipc::IPCResult RecvCrossProcessRedirect(
      RedirectToRealChannelArgs&& aArgs,
      CrossProcessRedirectResolver&& aResolve);

  mozilla::ipc::IPCResult RecvStartDelayedAutoplayMediaComponents(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvUpdateMediaControlKeysEvent(
      const MaybeDiscarded<BrowsingContext>& aContext,
      MediaControlKeysEvent aEvent);

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

  mozilla::ipc::IPCResult RecvDestroySHEntrySharedState(const uint64_t& aID);

  mozilla::ipc::IPCResult RecvEvictContentViewers(
      nsTArray<uint64_t>&& aToEvictSharedStateIDs);

  mozilla::ipc::IPCResult RecvSessionStorageData(
      uint64_t aTopContextId, const nsACString& aOriginAttrs,
      const nsACString& aOriginKey, const nsTArray<KeyValuePair>& aDefaultData,
      const nsTArray<KeyValuePair>& aSessionData);

  mozilla::ipc::IPCResult RecvUpdateSHEntriesInDocShell(
      CrossProcessSHEntry* aOldEntry, CrossProcessSHEntry* aNewEntry,
      const MaybeDiscarded<BrowsingContext>& aContext);

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

  const nsTArray<RefPtr<BrowsingContextGroup>>& BrowsingContextGroups() const {
    return mBrowsingContextGroupHolder;
  }

 private:
  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);
  void StartForceKillTimer();

  void ShutdownInternal();

  mozilla::ipc::IPCResult GetResultForRenderingInitFailure(
      base::ProcessId aOtherPid);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  virtual already_AddRefed<nsIEventTarget> GetSpecificMessageEventTarget(
      const Message& aMsg) override;

  virtual void OnChannelReceivedMessage(const Message& aMsg) override;

  mozilla::ipc::IPCResult RecvAttachBrowsingContext(
      BrowsingContext::IPCInitializer&& aInit);

  mozilla::ipc::IPCResult RecvDetachBrowsingContext(
      uint64_t aContextId, DetachBrowsingContextResolver&& aResolve);

  mozilla::ipc::IPCResult RecvCacheBrowsingContextChildren(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvRestoreBrowsingContextChildren(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const nsTArray<MaybeDiscarded<BrowsingContext>>& aChildren);

  mozilla::ipc::IPCResult RecvRegisterBrowsingContextGroup(
      nsTArray<BrowsingContext::IPCInitializer>&& aInits,
      nsTArray<WindowContext::IPCInitializer>&& aWindowInits);

  mozilla::ipc::IPCResult RecvWindowClose(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aTrustedCaller);
  mozilla::ipc::IPCResult RecvWindowFocus(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType);
  mozilla::ipc::IPCResult RecvWindowBlur(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvRaiseWindow(
      const MaybeDiscarded<BrowsingContext>& aContext, CallerType aCallerType);
  mozilla::ipc::IPCResult RecvClearFocus(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetFocusedBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvUnsetActiveBrowsingContext(
      const MaybeDiscarded<BrowsingContext>& aContext);
  mozilla::ipc::IPCResult RecvSetFocusedElement(
      const MaybeDiscarded<BrowsingContext>& aContext, bool aNeedsFocus);
  mozilla::ipc::IPCResult RecvBlurToChild(
      const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
      const MaybeDiscarded<BrowsingContext>& aBrowsingContextToClear,
      const MaybeDiscarded<BrowsingContext>& aAncestorBrowsingContextToFocus,
      bool aIsLeavingDocument, bool aAdjustWidget);
  mozilla::ipc::IPCResult RecvSetupFocusedAndActive(
      const MaybeDiscarded<BrowsingContext>& aFocusedBrowsingContext,
      const MaybeDiscarded<BrowsingContext>& aActiveBrowsingContext);
  mozilla::ipc::IPCResult RecvMaybeExitFullscreen(
      const MaybeDiscarded<BrowsingContext>& aContext);

  mozilla::ipc::IPCResult RecvWindowPostMessage(
      const MaybeDiscarded<BrowsingContext>& aContext,
      const ClonedMessageData& aMessage, const PostMessageData& aData);

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

  mozilla::ipc::IPCResult RecvLoadURI(
      const MaybeDiscarded<BrowsingContext>& aContext,
      nsDocShellLoadState* aLoadState, bool aSetNavigating);

  mozilla::ipc::IPCResult RecvInternalLoad(
      const MaybeDiscarded<BrowsingContext>& aContext,
      nsDocShellLoadState* aLoadState, bool aTakeFocus);

  mozilla::ipc::IPCResult RecvDisplayLoadError(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsAString& aURI);

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

  nsTArray<nsString> mAvailableDictionaries;

  // Temporary storage for a list of available fonts, passed from the
  // parent process and used to initialize gfx in the child. Currently used
  // only on MacOSX and Linux.
  nsTArray<mozilla::dom::SystemFontListEntry> mFontList;
  // Temporary storage for nsXPLookAndFeel flags.
  nsTArray<LookAndFeelInt> mLookAndFeelCache;

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
  nsString mRemoteType = VoidString();
  bool mIsAlive;
  nsString mProcessName;

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

  nsTArray<RefPtr<BrowsingContextGroup>> mBrowsingContextGroupHolder;

  // See `BrowsingContext::mEpochs` for an explanation of this field.
  uint64_t mBrowsingContextFieldEpoch = 0;

  DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

inline nsISupports* ToSupports(mozilla::dom::ContentChild* aContentChild) {
  return static_cast<nsIContentChild*>(aContentChild);
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ContentChild_h

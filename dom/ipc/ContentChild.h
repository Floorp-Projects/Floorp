/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/PBrowserOrId.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/CPOWManagerGetter.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nsRefPtrHashtable.h"

#include "nsIWindowProvider.h"

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
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

namespace mozilla {
class RemoteSpellcheckEngineChild;
class ChildProfilerController;

using mozilla::loader::PScriptCacheChild;

#if !defined(XP_WIN)
// Returns whether or not the currently running build is an unpackaged
// developer build. This check is implemented by looking for omni.ja in the
// the obj/dist dir. We use this routine to detect when the build dir will
// use symlinks to the repo and object dir. On Windows, dev builds don't
// use symlinks.
bool IsDevelopmentBuild();
#endif /* !XP_WIN */

#if defined(XP_MACOSX)
// Return the repo directory and the repo object directory respectively. These
// should only be used on Mac developer builds to determine the path to the
// repo or object directory.
nsresult GetRepoDir(nsIFile** aRepoDir);
nsresult GetObjDir(nsIFile** aObjDir);
#endif /* XP_MACOSX */

namespace ipc {
class URIParams;
}  // namespace ipc

namespace dom {

namespace ipc {
class SharedMap;
}

class AlertObserver;
class ConsoleListener;
class ClonedMessageData;
class TabChild;
class GetFilesHelperChild;
class FileCreatorHelper;

class ContentChild final : public PContentChild,
                           public nsIWindowProvider,
                           public CPOWManagerGetter,
                           public mozilla::ipc::IShmemAllocator {
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::ipc::FileDescriptor FileDescriptor;
  typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;
  typedef mozilla::ipc::URIParams URIParams;

  friend class PContentChild;

 public:
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
  };

  nsresult ProvideWindowCommon(TabChild* aTabOpener,
                               mozIDOMWindowProxy* aOpener, bool aIframeMoz,
                               uint32_t aChromeFlags, bool aCalledFromJS,
                               bool aPositionSpecified, bool aSizeSpecified,
                               nsIURI* aURI, const nsAString& aName,
                               const nsACString& aFeatures, bool aForceNoOpener,
                               nsDocShellLoadState* aLoadState,
                               bool* aWindowIsNew,
                               mozIDOMWindowProxy** aReturn);

  bool Init(MessageLoop* aIOLoop, base::ProcessId aParentPid,
            const char* aParentBuildID, IPC::Channel* aChannel,
            uint64_t aChildID, bool aIsForBrowser);

  void InitXPCOM(const XPCOMInitData& aXPCOMInit,
                 const mozilla::dom::ipc::StructuredCloneData& aInitialData);

  void InitGraphicsDeviceData(const ContentDeviceData& aData);

  static ContentChild* GetSingleton() { return sSingleton; }

  const AppInfo& GetAppInfo() { return mAppInfo; }

  void SetProcessName(const nsAString& aName);

  void GetProcessName(nsAString& aName) const;

  void GetProcessName(nsACString& aName) const;

  void LaunchRDDProcess();

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
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
      Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
      nsTArray<uint32_t>&& namespaces);

  mozilla::ipc::IPCResult RecvRequestPerformanceMetrics(const nsID& aID);

  mozilla::ipc::IPCResult RecvReinitRendering(
      Endpoint<PCompositorManagerChild>&& aCompositor,
      Endpoint<PImageBridgeChild>&& aImageBridge,
      Endpoint<PVRManagerChild>&& aVRBridge,
      Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
      nsTArray<uint32_t>&& namespaces);

  mozilla::ipc::IPCResult RecvAudioDefaultDeviceChange();

  mozilla::ipc::IPCResult RecvReinitRenderingForDeviceReset();

  mozilla::ipc::IPCResult RecvSetProcessSandbox(
      const Maybe<FileDescriptor>& aBroker);

  PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                    const TabId& aSameTabGroupAs,
                                    const IPCTabContext& aContext,
                                    const uint32_t& aChromeFlags,
                                    const ContentParentId& aCpID,
                                    BrowsingContext* aBrowsingContext,
                                    const bool& aIsForBrowser);

  bool DeallocPBrowserChild(PBrowserChild*);

  PIPCBlobInputStreamChild* AllocPIPCBlobInputStreamChild(
      const nsID& aID, const uint64_t& aSize);

  bool DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild* aActor);

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

  PChildToParentStreamChild* SendPChildToParentStreamConstructor(
      PChildToParentStreamChild*);

  PChildToParentStreamChild* AllocPChildToParentStreamChild();
  bool DeallocPChildToParentStreamChild(PChildToParentStreamChild*);

  PParentToChildStreamChild* AllocPParentToChildStreamChild();
  bool DeallocPParentToChildStreamChild(PParentToChildStreamChild*);

  PPSMContentDownloaderChild* AllocPPSMContentDownloaderChild(
      const uint32_t& aCertType);

  bool DeallocPPSMContentDownloaderChild(
      PPSMContentDownloaderChild* aDownloader);

  PExternalHelperAppChild* AllocPExternalHelperAppChild(
      const Maybe<URIParams>& uri,
      const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
      const nsCString& aMimeContentType, const nsCString& aContentDisposition,
      const uint32_t& aContentDispositionHint,
      const nsString& aContentDispositionFilename, const bool& aForceSave,
      const int64_t& aContentLength, const bool& aWasFileChannel,
      const Maybe<URIParams>& aReferrer, PBrowserChild* aBrowser);

  bool DeallocPExternalHelperAppChild(PExternalHelperAppChild* aService);

  PHandlerServiceChild* AllocPHandlerServiceChild();

  bool DeallocPHandlerServiceChild(PHandlerServiceChild*);

  PMediaChild* AllocPMediaChild();

  bool DeallocPMediaChild(PMediaChild* aActor);

  PPresentationChild* AllocPPresentationChild();

  bool DeallocPPresentationChild(PPresentationChild* aActor);

  mozilla::ipc::IPCResult RecvNotifyPresentationReceiverLaunched(
      PBrowserChild* aIframe, const nsString& aSessionId);

  mozilla::ipc::IPCResult RecvNotifyPresentationReceiverCleanUp(
      const nsString& aSessionId);

  mozilla::ipc::IPCResult RecvNotifyEmptyHTTPCache();

  PSpeechSynthesisChild* AllocPSpeechSynthesisChild();

  bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor);

  mozilla::ipc::IPCResult RecvRegisterChrome(
      InfallibleTArray<ChromePackage>&& packages,
      InfallibleTArray<SubstitutionMapping>&& resources,
      InfallibleTArray<OverrideMapping>&& overrides, const nsCString& locale,
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

  mozilla::ipc::IPCResult RecvNotifyVisited(nsTArray<URIParams>&& aURIs);

  // auto remove when alertfinished is received.
  nsresult AddRemoteAlertObserver(const nsString& aData,
                                  nsIObserver* aObserver);

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref);
  mozilla::ipc::IPCResult RecvVarUpdate(const GfxVarUpdate& pref);

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
                                           InfallibleTArray<CpowEntry>&& aCpows,
                                           const IPC::Principal& aPrincipal,
                                           const ClonedMessageData& aData);

  mozilla::ipc::IPCResult RecvRegisterStringBundles(
      nsTArray<StringBundleDescriptor>&& stringBundles);

  mozilla::ipc::IPCResult RecvUpdateSharedData(
      const FileDescriptor& aMapFile, const uint32_t& aMapSize,
      nsTArray<IPCBlob>&& aBlobs, nsTArray<nsCString>&& aChangedKeys);

  mozilla::ipc::IPCResult RecvGeolocationUpdate(nsIDOMGeoPosition* aPosition);

  mozilla::ipc::IPCResult RecvGeolocationError(const uint16_t& errorCode);

  mozilla::ipc::IPCResult RecvUpdateDictionaryList(
      InfallibleTArray<nsString>&& aDictionaries);

  mozilla::ipc::IPCResult RecvUpdateFontList(
      InfallibleTArray<SystemFontListEntry>&& aFontList);

  mozilla::ipc::IPCResult RecvUpdateAppLocales(
      nsTArray<nsCString>&& aAppLocales);
  mozilla::ipc::IPCResult RecvUpdateRequestedLocales(
      nsTArray<nsCString>&& aRequestedLocales);

  mozilla::ipc::IPCResult RecvClearSiteDataReloadNeeded(
      const nsString& aOrigin);

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
      const nsCString& sourceURL);

  mozilla::ipc::IPCResult RecvRemoteType(const nsString& aRemoteType);

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

  mozilla::ipc::IPCResult RecvLoadAndRegisterSheet(const URIParams& aURI,
                                                   const uint32_t& aType);

  mozilla::ipc::IPCResult RecvUnregisterSheet(const URIParams& aURI,
                                              const uint32_t& aType);

  void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  mozilla::ipc::IPCResult RecvNotifyIdleObserver(const uint64_t& aObserver,
                                                 const nsCString& aTopic,
                                                 const nsString& aData);

  mozilla::ipc::IPCResult RecvUpdateWindow(const uintptr_t& aChildId);

  mozilla::ipc::IPCResult RecvDomainSetChanged(const uint32_t& aSetType,
                                               const uint32_t& aChangeType,
                                               const Maybe<URIParams>& aDomain);

  mozilla::ipc::IPCResult RecvShutdown();

  mozilla::ipc::IPCResult RecvInvokeDragSession(
      nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction);

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
                                           InfallibleTArray<uint8_t>&& aData);

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

  // Get the directory for IndexedDB files. We query the parent for this and
  // cache the value
  nsString& GetIndexedDBPath();

  ContentParentId GetID() const { return mID; }

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  uint32_t GetChromeMainThreadId() const { return mMainChromeTid; }

  uint32_t GetMsaaID() const { return mMsaaID; }
#endif

  bool IsForBrowser() const { return mIsForBrowser; }

  PFileDescriptorSetChild* SendPFileDescriptorSetConstructor(
      const FileDescriptor&);

  PFileDescriptorSetChild* AllocPFileDescriptorSetChild(const FileDescriptor&);

  bool DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*);

  bool SendPBrowserConstructor(PBrowserChild* actor, const TabId& aTabId,
                               const TabId& aSameTabGroupAs,
                               const IPCTabContext& context,
                               const uint32_t& chromeFlags,
                               const ContentParentId& aCpID,
                               BrowsingContext* aBrowsingContext,
                               const bool& aIsForBrowser);

  virtual mozilla::ipc::IPCResult RecvPBrowserConstructor(
      PBrowserChild* aCctor, const TabId& aTabId, const TabId& aSameTabGroupAs,
      const IPCTabContext& aContext, const uint32_t& aChromeFlags,
      const ContentParentId& aCpID, BrowsingContext* aBrowsingContext,
      const bool& aIsForBrowser) override;

  FORWARD_SHMEM_ALLOCATOR_TO(PContentChild)

  void GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries);

  PBrowserOrId GetBrowserOrId(TabChild* aTabChild);

  POfflineCacheUpdateChild* AllocPOfflineCacheUpdateChild(
      const URIParams& manifestURI, const URIParams& documentURI,
      const PrincipalInfo& aLoadingPrincipalInfo, const bool& stickDocument);

  bool DeallocPOfflineCacheUpdateChild(
      POfflineCacheUpdateChild* offlineCacheUpdate);

  PWebrtcGlobalChild* AllocPWebrtcGlobalChild();

  bool DeallocPWebrtcGlobalChild(PWebrtcGlobalChild* aActor);

  PContentPermissionRequestChild* AllocPContentPermissionRequestChild(
      const InfallibleTArray<PermissionRequest>& aRequests,
      const IPC::Principal& aPrincipal,
      const IPC::Principal& aTopLevelPrincipal,
      const bool& aIsHandlingUserInput, const bool& aDocumentHasUserInput,
      const DOMTimeStamp aPageLoadTimestamp, const TabId& aTabId);
  bool DeallocPContentPermissionRequestChild(
      PContentPermissionRequestChild* actor);

  // Windows specific - set up audio session
  mozilla::ipc::IPCResult RecvSetAudioSessionData(const nsID& aId,
                                                  const nsString& aDisplayName,
                                                  const nsString& aIconPath);

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

  mozilla::ipc::IPCResult RecvFileCreationResponse(
      const nsID& aUUID, const FileCreationResult& aResult);

  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage, const Maybe<FileDescriptor>& DMDFile);

  mozilla::ipc::IPCResult RecvSetXPCOMProcessAttributes(
      const XPCOMInitData& aXPCOMInit, const StructuredCloneData& aInitialData,
      nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache,
      nsTArray<SystemFontListEntry>&& aFontList);

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
  InfallibleTArray<mozilla::dom::SystemFontListEntry>& SystemFontList() {
    return mFontList;
  }

  // PURLClassifierChild
  PURLClassifierChild* AllocPURLClassifierChild(const Principal& aPrincipal,
                                                bool* aSuccess);
  bool DeallocPURLClassifierChild(PURLClassifierChild* aActor);

  // PURLClassifierLocalChild
  PURLClassifierLocalChild* AllocPURLClassifierLocalChild(
      const URIParams& aUri,
      const nsTArray<IPCURLClassifierFeature>& aFeatures);
  bool DeallocPURLClassifierLocalChild(PURLClassifierLocalChild* aActor);

  PLoginReputationChild* AllocPLoginReputationChild(const URIParams& aUri);

  bool DeallocPLoginReputationChild(PLoginReputationChild* aActor);

  PSessionStorageObserverChild* AllocPSessionStorageObserverChild();

  bool DeallocPSessionStorageObserverChild(
      PSessionStorageObserverChild* aActor);

  nsTArray<LookAndFeelInt>& LookAndFeelCache() { return mLookAndFeelCache; }

  /**
   * Helper function for protocols that use the GPU process when available.
   * Overrides FatalError to just be a warning when communicating with the
   * GPU process since we don't want to crash the content process when the
   * GPU process crashes.
   */
  static void FatalErrorIfNotUsingGPUProcess(const char* const aErrorMsg,
                                             base::ProcessId aOtherPid);

  // This method is used by FileCreatorHelper for the creation of a BlobImpl.
  void FileCreationRequest(nsID& aUUID, FileCreatorHelper* aHelper,
                           const nsAString& aFullPath, const nsAString& aType,
                           const nsAString& aName,
                           const Optional<int64_t>& aLastModified,
                           bool aExistenceCheck, bool aIsFromNsIFile);

  typedef std::function<void(PRFileDesc*)> AnonymousTemporaryFileCallback;
  nsresult AsyncOpenAnonymousTemporaryFile(
      const AnonymousTemporaryFileCallback& aCallback);

  already_AddRefed<nsIEventTarget> GetEventTargetFor(TabChild* aTabChild);

  mozilla::ipc::IPCResult RecvSetPluginList(
      const uint32_t& aPluginEpoch, nsTArray<PluginTag>&& aPluginTags,
      nsTArray<FakePluginTag>&& aFakePluginTags);

  PClientOpenWindowOpChild* AllocPClientOpenWindowOpChild(
      const ClientOpenWindowArgs& aArgs);

  mozilla::ipc::IPCResult RecvPClientOpenWindowOpConstructor(
      PClientOpenWindowOpChild* aActor,
      const ClientOpenWindowArgs& aArgs) override;

  bool DeallocPClientOpenWindowOpChild(PClientOpenWindowOpChild* aActor);

  mozilla::ipc::IPCResult RecvSaveRecording(const FileDescriptor& aFile);

  mozilla::ipc::IPCResult RecvCrossProcessRedirect(
      const uint32_t& aRegistrarId, nsIURI* aURI, const uint32_t& aNewLoadFlags,
      const Maybe<LoadInfoArgs>& aLoadInfoForwarder, const uint64_t& aChannelId,
      nsIURI* aOriginalURI, const uint64_t& aIdentifier,
      const uint32_t& aRedirectMode);

#ifdef NIGHTLY_BUILD
  // Fetch the current number of pending input events.
  //
  // NOTE: This method performs an atomic read, and is safe to call from all
  // threads.
  uint32_t GetPendingInputEvents() { return mPendingInputEvents; }
#endif

 private:
  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);
  void StartForceKillTimer();

  void ShutdownInternal();

  mozilla::ipc::IPCResult GetResultForRenderingInitFailure(
      base::ProcessId aOtherPid);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  virtual already_AddRefed<nsIEventTarget> GetConstructedEventTarget(
      const Message& aMsg) override;

  virtual already_AddRefed<nsIEventTarget> GetSpecificMessageEventTarget(
      const Message& aMsg) override;

  virtual void OnChannelReceivedMessage(const Message& aMsg) override;

  mozilla::ipc::IPCResult RecvAttachBrowsingContext(
      BrowsingContext::IPCInitializer&& aInit);

  mozilla::ipc::IPCResult RecvDetachBrowsingContext(BrowsingContext* aContext,
                                                    bool aMoveToBFCache);

  mozilla::ipc::IPCResult RecvRegisterBrowsingContextGroup(
      nsTArray<BrowsingContext::IPCInitializer>&& aInits);

  mozilla::ipc::IPCResult RecvWindowClose(BrowsingContext* aContext,
                                          bool aTrustedCaller);
  mozilla::ipc::IPCResult RecvWindowFocus(BrowsingContext* aContext);
  mozilla::ipc::IPCResult RecvWindowBlur(BrowsingContext* aContext);
  mozilla::ipc::IPCResult RecvWindowPostMessage(
      BrowsingContext* aContext, const ClonedMessageData& aMessage,
      const PostMessageData& aData);

  mozilla::ipc::IPCResult RecvCommitBrowsingContextTransaction(
      BrowsingContext* aContext, BrowsingContext::Transaction&& aTransaction);

#ifdef NIGHTLY_BUILD
  virtual PContentChild::Result OnMessageReceived(const Message& aMsg) override;
#else
  using PContentChild::OnMessageReceived;
#endif

  virtual PContentChild::Result OnMessageReceived(const Message& aMsg,
                                                  Message*& aReply) override;

  InfallibleTArray<nsAutoPtr<AlertObserver>> mAlertObservers;
  RefPtr<ConsoleListener> mConsoleListener;

  nsTHashtable<nsPtrHashKey<nsIObserver>> mIdleObservers;

  InfallibleTArray<nsString> mAvailableDictionaries;

  // Temporary storage for a list of available fonts, passed from the
  // parent process and used to initialize gfx in the child. Currently used
  // only on MacOSX and Linux.
  InfallibleTArray<mozilla::dom::SystemFontListEntry> mFontList;
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
#endif

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

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
  nsCOMPtr<nsIFile> mProfileDir;
#endif

  // Hashtable to keep track of the pending GetFilesHelper objects.
  // This GetFilesHelperChild objects are removed when RecvGetFilesResponse is
  // received.
  nsRefPtrHashtable<nsIDHashKey, GetFilesHelperChild> mGetFilesPendingRequests;

  // Hashtable to keep track of the pending file creation.
  // These items are removed when RecvFileCreationResponse is received.
  nsRefPtrHashtable<nsIDHashKey, FileCreatorHelper> mFileCreationPending;

  nsClassHashtable<nsUint64HashKey, AnonymousTemporaryFileCallback>
      mPendingAnonymousTemporaryFiles;

  mozilla::Atomic<bool> mShuttingDown;

#ifdef NIGHTLY_BUILD
  // NOTE: This member is atomic because it can be accessed from
  // off-main-thread.
  mozilla::Atomic<uint32_t> mPendingInputEvents;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

uint64_t NextWindowID();

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ContentChild_h

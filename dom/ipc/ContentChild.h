/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PBrowserOrId.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"
#include "nsRefPtrHashtable.h"

#include "nsWeakPtr.h"
#include "nsIWindowProvider.h"

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
#include "nsIFile.h"
#endif

struct ChromePackage;
class nsIObserver;
struct SubstitutionMapping;
struct OverrideMapping;
class nsIDomainPolicy;
class nsIURIClassifierCallback;
struct LookAndFeelInt;
class nsDocShellLoadInfo;

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
nsresult GetRepoDir(nsIFile **aRepoDir);
nsresult GetObjDir(nsIFile **aObjDir);
#endif /* XP_MACOSX */

namespace ipc {
class OptionalURIParams;
class URIParams;
}// namespace ipc

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

class ContentChild final : public PContentChild
                         , public nsIWindowProvider
                         , public nsIContentChild
{
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::ipc::FileDescriptor FileDescriptor;
  typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
  typedef mozilla::ipc::PFileDescriptorSetChild PFileDescriptorSetChild;
  typedef mozilla::ipc::URIParams URIParams;

public:
  NS_DECL_NSIWINDOWPROVIDER

  ContentChild();
  virtual ~ContentChild();
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override { return 1; }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override { return 1; }

  struct AppInfo
  {
    nsCString version;
    nsCString buildID;
    nsCString name;
    nsCString UAName;
    nsCString ID;
    nsCString vendor;
    nsCString sourceURL;
  };

  nsresult
  ProvideWindowCommon(TabChild* aTabOpener,
                      mozIDOMWindowProxy* aOpener,
                      bool aIframeMoz,
                      uint32_t aChromeFlags,
                      bool aCalledFromJS,
                      bool aPositionSpecified,
                      bool aSizeSpecified,
                      nsIURI* aURI,
                      const nsAString& aName,
                      const nsACString& aFeatures,
                      bool aForceNoOpener,
                      nsDocShellLoadInfo* aLoadInfo,
                      bool* aWindowIsNew,
                      mozIDOMWindowProxy** aReturn);

  bool Init(MessageLoop* aIOLoop,
            base::ProcessId aParentPid,
            const char* aParentBuildID,
            IPC::Channel* aChannel,
            uint64_t aChildID,
            bool aIsForBrowser);

  void InitXPCOM(const XPCOMInitData& aXPCOMInit,
                 const mozilla::dom::ipc::StructuredCloneData& aInitialData);

  void InitGraphicsDeviceData(const ContentDeviceData& aData);

  static ContentChild* GetSingleton()
  {
    return sSingleton;
  }

  const AppInfo& GetAppInfo()
  {
    return mAppInfo;
  }

  void SetProcessName(const nsAString& aName);

  void GetProcessName(nsAString& aName) const;

  void GetProcessName(nsACString& aName) const;

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
  void GetProfileDir(nsIFile** aProfileDir) const
  {
    *aProfileDir = mProfileDir;
    NS_IF_ADDREF(*aProfileDir);
  }

  void SetProfileDir(nsIFile* aProfileDir)
  {
    mProfileDir = aProfileDir;
  }
#endif

  bool IsAlive() const;

  bool IsShuttingDown() const;

  ipc::SharedMap* SharedData() { return mSharedData; };

  static void AppendProcessId(nsACString& aName);

  static void UpdateCookieStatus(nsIChannel *aChannel);

  mozilla::ipc::IPCResult
  RecvInitContentBridgeChild(Endpoint<PContentBridgeChild>&& aEndpoint) override;

  mozilla::ipc::IPCResult
  RecvInitGMPService(Endpoint<PGMPServiceChild>&& aGMPService) override;

  mozilla::ipc::IPCResult
  RecvInitProfiler(Endpoint<PProfilerChild>&& aEndpoint) override;

  mozilla::ipc::IPCResult
  RecvGMPsChanged(nsTArray<GMPCapabilityData>&& capabilities) override;

  mozilla::ipc::IPCResult
  RecvInitProcessHangMonitor(Endpoint<PProcessHangMonitorChild>&& aHangMonitor) override;

  mozilla::ipc::IPCResult
  RecvInitRendering(
    Endpoint<PCompositorManagerChild>&& aCompositor,
    Endpoint<PImageBridgeChild>&& aImageBridge,
    Endpoint<PVRManagerChild>&& aVRBridge,
    Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
    nsTArray<uint32_t>&& namespaces) override;

  mozilla::ipc::IPCResult
  RecvRequestPerformanceMetrics(const nsID& aID) override;

  mozilla::ipc::IPCResult
  RecvReinitRendering(
    Endpoint<PCompositorManagerChild>&& aCompositor,
    Endpoint<PImageBridgeChild>&& aImageBridge,
    Endpoint<PVRManagerChild>&& aVRBridge,
    Endpoint<PVideoDecoderManagerChild>&& aVideoManager,
    nsTArray<uint32_t>&& namespaces) override;

  virtual mozilla::ipc::IPCResult RecvAudioDefaultDeviceChange() override;

  mozilla::ipc::IPCResult RecvReinitRenderingForDeviceReset() override;

  virtual mozilla::ipc::IPCResult RecvSetProcessSandbox(const MaybeFileDesc& aBroker) override;

  virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                            const TabId& aSameTabGroupAs,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForBrowser) override;

  virtual bool DeallocPBrowserChild(PBrowserChild*) override;

  virtual PIPCBlobInputStreamChild*
  AllocPIPCBlobInputStreamChild(const nsID& aID,
                                const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamChild(PIPCBlobInputStreamChild* aActor) override;

  virtual PHalChild* AllocPHalChild() override;
  virtual bool DeallocPHalChild(PHalChild*) override;

  virtual PHeapSnapshotTempFileHelperChild*
  AllocPHeapSnapshotTempFileHelperChild() override;

  virtual bool
  DeallocPHeapSnapshotTempFileHelperChild(PHeapSnapshotTempFileHelperChild*) override;

  virtual PCycleCollectWithLogsChild*
  AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                  const FileDescriptor& aGCLog,
                                  const FileDescriptor& aCCLog) override;

  virtual bool
  DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aChild,
                                       const bool& aDumpAllTraces,
                                       const FileDescriptor& aGCLog,
                                       const FileDescriptor& aCCLog) override;

  virtual PWebBrowserPersistDocumentChild*
  AllocPWebBrowserPersistDocumentChild(PBrowserChild* aBrowser,
                                       const uint64_t& aOuterWindowID) override;

  virtual mozilla::ipc::IPCResult
  RecvPWebBrowserPersistDocumentConstructor(PWebBrowserPersistDocumentChild *aActor,
                                            PBrowserChild *aBrowser,
                                            const uint64_t& aOuterWindowID) override;

  virtual bool
  DeallocPWebBrowserPersistDocumentChild(PWebBrowserPersistDocumentChild* aActor) override;

  virtual PTestShellChild* AllocPTestShellChild() override;

  virtual bool DeallocPTestShellChild(PTestShellChild*) override;

  virtual mozilla::ipc::IPCResult RecvPTestShellConstructor(PTestShellChild*) override;

  virtual PScriptCacheChild*
  AllocPScriptCacheChild(const FileDescOrError& cacheFile,
                         const bool& wantCacheData) override;

  virtual bool DeallocPScriptCacheChild(PScriptCacheChild*) override;

  virtual mozilla::ipc::IPCResult
  RecvPScriptCacheConstructor(PScriptCacheChild*,
                              const FileDescOrError& cacheFile,
                              const bool& wantCacheData) override;

  jsipc::CPOWManager* GetCPOWManager() override;

  virtual PNeckoChild* AllocPNeckoChild() override;

  virtual bool DeallocPNeckoChild(PNeckoChild*) override;

  virtual PPrintingChild* AllocPPrintingChild() override;

  virtual bool DeallocPPrintingChild(PPrintingChild*) override;

  virtual PChildToParentStreamChild*
  SendPChildToParentStreamConstructor(PChildToParentStreamChild*) override;

  virtual PChildToParentStreamChild* AllocPChildToParentStreamChild() override;
  virtual bool DeallocPChildToParentStreamChild(PChildToParentStreamChild*) override;

  virtual PParentToChildStreamChild* AllocPParentToChildStreamChild() override;
  virtual bool DeallocPParentToChildStreamChild(PParentToChildStreamChild*) override;

  virtual PPSMContentDownloaderChild*
  AllocPPSMContentDownloaderChild( const uint32_t& aCertType) override;

  virtual bool
  DeallocPPSMContentDownloaderChild(PPSMContentDownloaderChild* aDownloader) override;

  virtual PExternalHelperAppChild*
  AllocPExternalHelperAppChild(const OptionalURIParams& uri,
                               const nsCString& aMimeContentType,
                               const nsCString& aContentDisposition,
                               const uint32_t& aContentDispositionHint,
                               const nsString& aContentDispositionFilename,
                               const bool& aForceSave,
                               const int64_t& aContentLength,
                               const bool& aWasFileChannel,
                               const OptionalURIParams& aReferrer,
                               PBrowserChild* aBrowser) override;

  virtual bool
  DeallocPExternalHelperAppChild(PExternalHelperAppChild *aService) override;

  virtual PHandlerServiceChild* AllocPHandlerServiceChild() override;

  virtual bool DeallocPHandlerServiceChild(PHandlerServiceChild*) override;

  virtual PMediaChild* AllocPMediaChild() override;

  virtual bool DeallocPMediaChild(PMediaChild* aActor) override;

  virtual PPresentationChild* AllocPPresentationChild() override;

  virtual bool DeallocPPresentationChild(PPresentationChild* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyPresentationReceiverLaunched(PBrowserChild* aIframe,
                                         const nsString& aSessionId) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyPresentationReceiverCleanUp(const nsString& aSessionId) override;

  virtual mozilla::ipc::IPCResult RecvNotifyEmptyHTTPCache() override;

  virtual PSpeechSynthesisChild* AllocPSpeechSynthesisChild() override;

  virtual bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) override;

  virtual mozilla::ipc::IPCResult RecvRegisterChrome(InfallibleTArray<ChromePackage>&& packages,
                                                     InfallibleTArray<SubstitutionMapping>&& resources,
                                                     InfallibleTArray<OverrideMapping>&& overrides,
                                                     const nsCString& locale,
                                                     const bool& reset) override;
  virtual mozilla::ipc::IPCResult RecvRegisterChromeItem(const ChromeRegistryItem& item) override;

  virtual mozilla::ipc::IPCResult RecvClearImageCache(const bool& privateLoader,
                                                      const bool& chrome) override;

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() override;

  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) override;

  virtual PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild() override;

  virtual bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*) override;

  virtual mozilla::ipc::IPCResult RecvSetOffline(const bool& offline) override;

  virtual mozilla::ipc::IPCResult RecvSetConnectivity(const bool& connectivity) override;
  virtual mozilla::ipc::IPCResult RecvSetCaptivePortalState(const int32_t& state) override;

  virtual mozilla::ipc::IPCResult RecvBidiKeyboardNotify(const bool& isLangRTL,
                                                         const bool& haveBidiKeyboards) override;

  virtual mozilla::ipc::IPCResult RecvNotifyVisited(nsTArray<URIParams>&& aURIs) override;

  // auto remove when alertfinished is received.
  nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

  virtual mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref) override;
  virtual mozilla::ipc::IPCResult RecvVarUpdate(const GfxVarUpdate& pref) override;

  virtual mozilla::ipc::IPCResult RecvDataStoragePut(const nsString& aFilename,
                                                     const DataStorageItem& aItem) override;

  virtual mozilla::ipc::IPCResult RecvDataStorageRemove(const nsString& aFilename,
                                                        const nsCString& aKey,
                                                        const DataStorageType& aType) override;

  virtual mozilla::ipc::IPCResult RecvDataStorageClear(const nsString& aFilename) override;

  virtual mozilla::ipc::IPCResult RecvNotifyAlertsObserver(const nsCString& aType,
                                                           const nsString& aData) override;

  virtual mozilla::ipc::IPCResult RecvLoadProcessScript(const nsString& aURL) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  mozilla::ipc::IPCResult RecvRegisterStringBundles(nsTArray<StringBundleDescriptor>&& stringBundles) override;

  mozilla::ipc::IPCResult RecvUpdateSharedData(const FileDescriptor& aMapFile,
                                               const uint32_t& aMapSize,
                                               nsTArray<IPCBlob>&& aBlobs,
                                               nsTArray<nsCString>&& aChangedKeys) override;

  virtual mozilla::ipc::IPCResult RecvGeolocationUpdate(nsIDOMGeoPosition* aPosition) override;

  virtual mozilla::ipc::IPCResult RecvGeolocationError(const uint16_t& errorCode) override;

  virtual mozilla::ipc::IPCResult RecvUpdateDictionaryList(InfallibleTArray<nsString>&& aDictionaries) override;

  virtual mozilla::ipc::IPCResult RecvUpdateFontList(InfallibleTArray<SystemFontListEntry>&& aFontList) override;

  virtual mozilla::ipc::IPCResult RecvUpdateAppLocales(nsTArray<nsCString>&& aAppLocales) override;
  virtual mozilla::ipc::IPCResult RecvUpdateRequestedLocales(nsTArray<nsCString>&& aRequestedLocales) override;

  virtual mozilla::ipc::IPCResult RecvClearSiteDataReloadNeeded(const nsString& aOrigin) override;

  virtual mozilla::ipc::IPCResult RecvAddPermission(const IPC::Permission& permission) override;

  virtual mozilla::ipc::IPCResult RecvRemoveAllPermissions() override;

  virtual mozilla::ipc::IPCResult RecvFlushMemory(const nsString& reason) override;

  virtual mozilla::ipc::IPCResult RecvActivateA11y(const uint32_t& aMainChromeTid,
                                                   const uint32_t& aMsaaID) override;
  virtual mozilla::ipc::IPCResult RecvShutdownA11y() override;

  virtual mozilla::ipc::IPCResult RecvGarbageCollect() override;
  virtual mozilla::ipc::IPCResult RecvCycleCollect() override;
  virtual mozilla::ipc::IPCResult RecvUnlinkGhosts() override;

  virtual mozilla::ipc::IPCResult RecvAppInfo(const nsCString& version, const nsCString& buildID,
                                              const nsCString& name, const nsCString& UAName,
                                              const nsCString& ID, const nsCString& vendor,
                                              const nsCString& sourceURL) override;

  virtual mozilla::ipc::IPCResult RecvRemoteType(const nsString& aRemoteType) override;

  const nsAString& GetRemoteType() const;

  virtual mozilla::ipc::IPCResult
  RecvInitServiceWorkers(const ServiceWorkerConfiguration& aConfig) override;

  virtual mozilla::ipc::IPCResult
  RecvInitBlobURLs(nsTArray<BlobURLRegistrationData>&& aRegistations) override;

  virtual mozilla::ipc::IPCResult RecvLastPrivateDocShellDestroyed() override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority) override;

  virtual mozilla::ipc::IPCResult RecvMinimizeMemoryUsage() override;

  virtual mozilla::ipc::IPCResult RecvLoadAndRegisterSheet(const URIParams& aURI,
                                                           const uint32_t& aType) override;

  virtual mozilla::ipc::IPCResult RecvUnregisterSheet(const URIParams& aURI,
                                                      const uint32_t& aType) override;

  void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  virtual mozilla::ipc::IPCResult RecvNotifyIdleObserver(const uint64_t& aObserver,
                                                         const nsCString& aTopic,
                                                         const nsString& aData) override;

  virtual mozilla::ipc::IPCResult RecvUpdateWindow(const uintptr_t& aChildId) override;

  virtual mozilla::ipc::IPCResult RecvDomainSetChanged(const uint32_t& aSetType,
                                                       const uint32_t& aChangeType,
                                                       const OptionalURIParams& aDomain) override;

  virtual mozilla::ipc::IPCResult RecvShutdown() override;

  virtual mozilla::ipc::IPCResult
  RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                        const uint32_t& aAction) override;

  virtual mozilla::ipc::IPCResult RecvEndDragSession(const bool& aDoneDrag,
                                                     const bool& aUserCancelled,
                                                     const mozilla::LayoutDeviceIntPoint& aEndDragPoint,
                                                     const uint32_t& aKeyModifiers) override;

  virtual mozilla::ipc::IPCResult
  RecvPush(const nsCString& aScope,
           const IPC::Principal& aPrincipal,
           const nsString& aMessageId) override;

  virtual mozilla::ipc::IPCResult
  RecvPushWithData(const nsCString& aScope,
                   const IPC::Principal& aPrincipal,
                   const nsString& aMessageId,
                   InfallibleTArray<uint8_t>&& aData) override;

  virtual mozilla::ipc::IPCResult
  RecvPushSubscriptionChange(const nsCString& aScope,
                             const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult
  RecvPushError(const nsCString& aScope, const IPC::Principal& aPrincipal,
                const nsString& aMessage, const uint32_t& aFlags) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                              const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvActivate(PBrowserChild* aTab) override;

  virtual mozilla::ipc::IPCResult RecvDeactivate(PBrowserChild* aTab) override;

  mozilla::ipc::IPCResult
  RecvRefreshScreens(nsTArray<ScreenDetails>&& aScreens) override;

  // Get the directory for IndexedDB files. We query the parent for this and
  // cache the value
  nsString &GetIndexedDBPath();

  ContentParentId GetID() const { return mID; }

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  uint32_t GetChromeMainThreadId() const { return mMainChromeTid; }

  uint32_t GetMsaaID() const { return mMsaaID; }
#endif

  bool IsForBrowser() const { return mIsForBrowser; }

  virtual PFileDescriptorSetChild*
  SendPFileDescriptorSetConstructor(const FileDescriptor&) override;

  virtual PFileDescriptorSetChild*
  AllocPFileDescriptorSetChild(const FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*) override;

  virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                       const TabId& aTabId,
                                       const TabId& aSameTabGroupAs,
                                       const IPCTabContext& context,
                                       const uint32_t& chromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForBrowser) override;

  virtual mozilla::ipc::IPCResult RecvPBrowserConstructor(PBrowserChild* aCctor,
                                                          const TabId& aTabId,
                                                          const TabId& aSameTabGroupAs,
                                                          const IPCTabContext& aContext,
                                                          const uint32_t& aChromeFlags,
                                                          const ContentParentId& aCpID,
                                                          const bool& aIsForBrowser) override;

  FORWARD_SHMEM_ALLOCATOR_TO(PContentChild)

  void GetAvailableDictionaries(InfallibleTArray<nsString>& aDictionaries);

  PBrowserOrId
  GetBrowserOrId(TabChild* aTabChild);

  virtual POfflineCacheUpdateChild*
  AllocPOfflineCacheUpdateChild(const URIParams& manifestURI,
                                const URIParams& documentURI,
                                const PrincipalInfo& aLoadingPrincipalInfo,
                                const bool& stickDocument) override;

  virtual bool
  DeallocPOfflineCacheUpdateChild(POfflineCacheUpdateChild* offlineCacheUpdate) override;

  virtual PWebrtcGlobalChild* AllocPWebrtcGlobalChild() override;

  virtual bool DeallocPWebrtcGlobalChild(PWebrtcGlobalChild *aActor) override;

  virtual PContentPermissionRequestChild*
  AllocPContentPermissionRequestChild(const InfallibleTArray<PermissionRequest>& aRequests,
                                      const IPC::Principal& aPrincipal,
                                      const bool& aIsHandlingUserInput,
                                      const TabId& aTabId) override;
  virtual bool
  DeallocPContentPermissionRequestChild(PContentPermissionRequestChild* actor) override;

  // Windows specific - set up audio session
  virtual mozilla::ipc::IPCResult
  RecvSetAudioSessionData(const nsID& aId,
                          const nsString& aDisplayName,
                          const nsString& aIconPath) override;


  // GetFiles for WebKit/Blink FileSystem API and Directory API must run on the
  // parent process.
  void
  CreateGetFilesRequest(const nsAString& aDirectoryPath, bool aRecursiveFlag,
                        nsID& aUUID, GetFilesHelperChild* aChild);

  void
  DeleteGetFilesRequest(nsID& aUUID, GetFilesHelperChild* aChild);

  virtual mozilla::ipc::IPCResult
  RecvGetFilesResponse(const nsID& aUUID,
                       const GetFilesResponseResult& aResult) override;

  virtual mozilla::ipc::IPCResult
  RecvBlobURLRegistration(const nsCString& aURI, const IPCBlob& aBlob,
                          const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult
  RecvBlobURLUnregistration(const nsCString& aURI) override;

  virtual mozilla::ipc::IPCResult
  RecvFileCreationResponse(const nsID& aUUID,
                           const FileCreationResult& aResult) override;

  mozilla::ipc::IPCResult
  RecvRequestMemoryReport(
          const uint32_t& generation,
          const bool& anonymize,
          const bool& minimizeMemoryUsage,
          const MaybeFileDesc& DMDFile) override;

  virtual mozilla::ipc::IPCResult
  RecvSetXPCOMProcessAttributes(const XPCOMInitData& aXPCOMInit,
                                const StructuredCloneData& aInitialData,
                                nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache,
                                nsTArray<SystemFontListEntry>&& aFontList) override;

  virtual mozilla::ipc::IPCResult
  RecvProvideAnonymousTemporaryFile(const uint64_t& aID, const FileDescOrError& aFD) override;

  mozilla::ipc::IPCResult
  RecvSetPermissionsWithKey(const nsCString& aPermissionKey,
                            nsTArray<IPC::Permission>&& aPerms) override;

  virtual mozilla::ipc::IPCResult
  RecvShareCodeCoverageMutex(const CrossProcessMutexHandle& aHandle) override;

  virtual mozilla::ipc::IPCResult
  RecvFlushCodeCoverageCounters(FlushCodeCoverageCountersResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult
  RecvSetInputEventQueueEnabled() override;

  virtual mozilla::ipc::IPCResult
  RecvFlushInputEventQueue() override;

  virtual mozilla::ipc::IPCResult
  RecvSuspendInputEventQueue() override;

  virtual mozilla::ipc::IPCResult
  RecvResumeInputEventQueue() override;

  virtual mozilla::ipc::IPCResult
  RecvAddDynamicScalars(nsTArray<DynamicScalarDefinition>&& aDefs) override;

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  bool
  SendGetA11yContentId();
#endif // defined(XP_WIN) && defined(ACCESSIBILITY)

  // Get a reference to the font list passed from the chrome process,
  // for use during gfx initialization.
  InfallibleTArray<mozilla::dom::SystemFontListEntry>&
  SystemFontList() {
    return mFontList;
  }

  // PURLClassifierChild
  virtual PURLClassifierChild*
  AllocPURLClassifierChild(const Principal& aPrincipal,
                           const bool& aUseTrackingProtection,
                           bool* aSuccess) override;
  virtual bool
  DeallocPURLClassifierChild(PURLClassifierChild* aActor) override;

  // PURLClassifierLocalChild
  virtual PURLClassifierLocalChild*
  AllocPURLClassifierLocalChild(const URIParams& aUri,
                                const nsCString& aTables) override;
  virtual bool
  DeallocPURLClassifierLocalChild(PURLClassifierLocalChild* aActor) override;

  virtual PLoginReputationChild*
  AllocPLoginReputationChild(const URIParams& aUri) override;

  virtual bool
  DeallocPLoginReputationChild(PLoginReputationChild* aActor) override;

  nsTArray<LookAndFeelInt>&
  LookAndFeelCache() {
    return mLookAndFeelCache;
  }

  /**
   * Helper function for protocols that use the GPU process when available.
   * Overrides FatalError to just be a warning when communicating with the
   * GPU process since we don't want to crash the content process when the
   * GPU process crashes.
   */
  static void FatalErrorIfNotUsingGPUProcess(const char* const aErrorMsg,
                                             base::ProcessId aOtherPid);

  // This method is used by FileCreatorHelper for the creation of a BlobImpl.
  void
  FileCreationRequest(nsID& aUUID, FileCreatorHelper* aHelper,
                      const nsAString& aFullPath, const nsAString& aType,
                      const nsAString& aName,
                      const Optional<int64_t>& aLastModified,
                      bool aExistenceCheck, bool aIsFromNsIFile);

  typedef std::function<void(PRFileDesc*)> AnonymousTemporaryFileCallback;
  nsresult AsyncOpenAnonymousTemporaryFile(const AnonymousTemporaryFileCallback& aCallback);

  virtual already_AddRefed<nsIEventTarget> GetEventTargetFor(TabChild* aTabChild) override;

  mozilla::ipc::IPCResult
  RecvSetPluginList(const uint32_t& aPluginEpoch,
                    nsTArray<PluginTag>&& aPluginTags,
                    nsTArray<FakePluginTag>&& aFakePluginTags) override;

  virtual PClientOpenWindowOpChild*
  AllocPClientOpenWindowOpChild(const ClientOpenWindowArgs& aArgs) override;

  virtual mozilla::ipc::IPCResult
  RecvPClientOpenWindowOpConstructor(PClientOpenWindowOpChild* aActor,
                                     const ClientOpenWindowArgs& aArgs) override;

  virtual bool
  DeallocPClientOpenWindowOpChild(PClientOpenWindowOpChild* aActor) override;

  mozilla::ipc::IPCResult
  RecvSaveRecording(const FileDescriptor& aFile) override;

#ifdef NIGHTLY_BUILD
  // Fetch the current number of pending input events.
  //
  // NOTE: This method performs an atomic read, and is safe to call from all threads.
  uint32_t
  GetPendingInputEvents()
  {
    return mPendingInputEvents;
  }
#endif

private:
  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);
  void StartForceKillTimer();

  void ShutdownInternal();

  mozilla::ipc::IPCResult
  GetResultForRenderingInitFailure(base::ProcessId aOtherPid);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  virtual already_AddRefed<nsIEventTarget>
  GetConstructedEventTarget(const Message& aMsg) override;

  virtual already_AddRefed<nsIEventTarget>
  GetSpecificMessageEventTarget(const Message& aMsg) override;

#ifdef NIGHTLY_BUILD
  virtual void
  OnChannelReceivedMessage(const Message& aMsg) override;

  virtual PContentChild::Result
  OnMessageReceived(const Message& aMsg) override;

  virtual PContentChild::Result
  OnMessageReceived(const Message& aMsg, Message*& aReply) override;
#endif

  InfallibleTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
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


  nsClassHashtable<nsUint64HashKey, AnonymousTemporaryFileCallback> mPendingAnonymousTemporaryFiles;

  mozilla::Atomic<bool> mShuttingDown;

#ifdef NIGHTLY_BUILD
  // NOTE: This member is atomic because it can be accessed from off-main-thread.
  mozilla::Atomic<uint32_t> mPendingInputEvents;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

uint64_t
NextWindowID();

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContentChild_h

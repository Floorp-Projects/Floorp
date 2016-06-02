/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentChild_h
#define mozilla_dom_ContentChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/nsIContentChild.h"
#include "mozilla/dom/PBrowserOrId.h"
#include "mozilla/dom/PContentChild.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"

#include "nsWeakPtr.h"
#include "nsIWindowProvider.h"


struct ChromePackage;
class nsIObserver;
struct SubstitutionMapping;
struct OverrideMapping;
class nsIDomainPolicy;

namespace mozilla {
class RemoteSpellcheckEngineChild;

namespace ipc {
class OptionalURIParams;
class PFileDescriptorSetChild;
class URIParams;
}// namespace ipc

namespace layers {
class PCompositorBridgeChild;
} // namespace layers

namespace dom {

class AlertObserver;
class ConsoleListener;
class PStorageChild;
class ClonedMessageData;
class TabChild;

class ContentChild final : public PContentChild
                         , public nsIWindowProvider
                         , public nsIContentChild
{
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
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
                      bool* aWindowIsNew,
                      mozIDOMWindowProxy** aReturn);

  bool Init(MessageLoop* aIOLoop,
            base::ProcessId aParentPid,
            IPC::Channel* aChannel);

  void InitProcessAttributes();

  void InitXPCOM();

  void InitGraphicsDeviceData();

  static ContentChild* GetSingleton()
  {
    return sSingleton;
  }

  const AppInfo& GetAppInfo()
  {
    return mAppInfo;
  }

  void SetProcessName(const nsAString& aName, bool aDontOverride = false);

  void GetProcessName(nsAString& aName) const;

  void GetProcessName(nsACString& aName) const;

  bool IsAlive() const;

  static void AppendProcessId(nsACString& aName);

  ContentBridgeParent* GetLastBridge()
  {
    MOZ_ASSERT(mLastBridge);
    ContentBridgeParent* parent = mLastBridge;
    mLastBridge = nullptr;
    return parent;
  }

  RefPtr<ContentBridgeParent> mLastBridge;

  PPluginModuleParent *
  AllocPPluginModuleParent(mozilla::ipc::Transport* transport,
                           base::ProcessId otherProcess) override;

  PContentBridgeParent*
  AllocPContentBridgeParent(mozilla::ipc::Transport* transport,
                            base::ProcessId otherProcess) override;
  PContentBridgeChild*
  AllocPContentBridgeChild(mozilla::ipc::Transport* transport,
                           base::ProcessId otherProcess) override;

  PGMPServiceChild*
  AllocPGMPServiceChild(mozilla::ipc::Transport* transport,
                        base::ProcessId otherProcess) override;

  PAPZChild*
  AllocPAPZChild(const TabId& aTabId) override;
  bool
  DeallocPAPZChild(PAPZChild* aActor) override;

  PCompositorBridgeChild*
  AllocPCompositorBridgeChild(mozilla::ipc::Transport* aTransport,
                              base::ProcessId aOtherProcess) override;

  PSharedBufferManagerChild*
  AllocPSharedBufferManagerChild(mozilla::ipc::Transport* aTransport,
                                  base::ProcessId aOtherProcess) override;

  PImageBridgeChild*
  AllocPImageBridgeChild(mozilla::ipc::Transport* aTransport,
                         base::ProcessId aOtherProcess) override;

  PProcessHangMonitorChild*
  AllocPProcessHangMonitorChild(Transport* aTransport,
                                ProcessId aOtherProcess) override;

  PVRManagerChild*
  AllocPVRManagerChild(Transport* aTransport,
                       ProcessId aOtherProcess) override;

  virtual bool RecvSetProcessSandbox(const MaybeFileDesc& aBroker) override;

  PBackgroundChild*
  AllocPBackgroundChild(Transport* aTransport, ProcessId aOtherProcess)
                        override;

  virtual PBrowserChild* AllocPBrowserChild(const TabId& aTabId,
                                            const IPCTabContext& aContext,
                                            const uint32_t& aChromeFlags,
                                            const ContentParentId& aCpID,
                                            const bool& aIsForApp,
                                            const bool& aIsForBrowser)
                                            override;

  virtual bool DeallocPBrowserChild(PBrowserChild*) override;

  virtual PDeviceStorageRequestChild*
  AllocPDeviceStorageRequestChild(const DeviceStorageParams&) override;

  virtual bool
  DeallocPDeviceStorageRequestChild(PDeviceStorageRequestChild*) override;

  virtual PBlobChild*
  AllocPBlobChild(const BlobConstructorParams& aParams) override;

  virtual bool DeallocPBlobChild(PBlobChild* aActor) override;

  virtual PCrashReporterChild*
  AllocPCrashReporterChild(const mozilla::dom::NativeThreadId& id,
                           const uint32_t& processType) override;

  virtual bool
  DeallocPCrashReporterChild(PCrashReporterChild*) override;

  virtual PHalChild* AllocPHalChild() override;
  virtual bool DeallocPHalChild(PHalChild*) override;

  virtual PHeapSnapshotTempFileHelperChild*
  AllocPHeapSnapshotTempFileHelperChild() override;

  virtual bool
  DeallocPHeapSnapshotTempFileHelperChild(PHeapSnapshotTempFileHelperChild*) override;

  PIccChild*
  SendPIccConstructor(PIccChild* aActor, const uint32_t& aServiceId);

  virtual PIccChild* AllocPIccChild(const uint32_t& aClientId) override;

  virtual bool DeallocPIccChild(PIccChild* aActor) override;

  virtual PMemoryReportRequestChild*
  AllocPMemoryReportRequestChild(const uint32_t& aGeneration,
                                 const bool& aAnonymize,
                                 const bool& aMinimizeMemoryUsage,
                                 const MaybeFileDesc& aDMDFile) override;

  virtual bool
  DeallocPMemoryReportRequestChild(PMemoryReportRequestChild* actor) override;

  virtual bool
  RecvPMemoryReportRequestConstructor(PMemoryReportRequestChild* aChild,
                                      const uint32_t& aGeneration,
                                      const bool& aAnonymize,
                                      const bool &aMinimizeMemoryUsage,
                                      const MaybeFileDesc &aDMDFile) override;

  virtual PCycleCollectWithLogsChild*
  AllocPCycleCollectWithLogsChild(const bool& aDumpAllTraces,
                                  const FileDescriptor& aGCLog,
                                  const FileDescriptor& aCCLog) override;

  virtual bool
  DeallocPCycleCollectWithLogsChild(PCycleCollectWithLogsChild* aActor) override;

  virtual bool
  RecvPCycleCollectWithLogsConstructor(PCycleCollectWithLogsChild* aChild,
                                       const bool& aDumpAllTraces,
                                       const FileDescriptor& aGCLog,
                                       const FileDescriptor& aCCLog) override;

  virtual PWebBrowserPersistDocumentChild*
  AllocPWebBrowserPersistDocumentChild(PBrowserChild* aBrowser,
                                       const uint64_t& aOuterWindowID) override;

  virtual bool
  RecvPWebBrowserPersistDocumentConstructor(PWebBrowserPersistDocumentChild *aActor,
                                            PBrowserChild *aBrowser,
                                            const uint64_t& aOuterWindowID) override;

  virtual bool
  DeallocPWebBrowserPersistDocumentChild(PWebBrowserPersistDocumentChild* aActor) override;

  virtual PTestShellChild* AllocPTestShellChild() override;

  virtual bool DeallocPTestShellChild(PTestShellChild*) override;

  virtual bool RecvPTestShellConstructor(PTestShellChild*) override;

  jsipc::CPOWManager* GetCPOWManager() override;

  PMobileConnectionChild*
  SendPMobileConnectionConstructor(PMobileConnectionChild* aActor,
                                   const uint32_t& aClientId);

  virtual PMobileConnectionChild*
  AllocPMobileConnectionChild(const uint32_t& aClientId) override;

  virtual bool
  DeallocPMobileConnectionChild(PMobileConnectionChild* aActor) override;

  virtual PNeckoChild* AllocPNeckoChild() override;

  virtual bool DeallocPNeckoChild(PNeckoChild*) override;

  virtual PPrintingChild* AllocPPrintingChild() override;

  virtual bool DeallocPPrintingChild(PPrintingChild*) override;

  virtual PSendStreamChild* AllocPSendStreamChild() override;
  virtual bool DeallocPSendStreamChild(PSendStreamChild*) override;

  virtual PScreenManagerChild*
  AllocPScreenManagerChild(uint32_t* aNumberOfScreens,
                           float* aSystemDefaultScale,
                           bool* aSuccess) override;

  virtual bool DeallocPScreenManagerChild(PScreenManagerChild*) override;

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
                               const OptionalURIParams& aReferrer,
                               PBrowserChild* aBrowser) override;

  virtual bool
  DeallocPExternalHelperAppChild(PExternalHelperAppChild *aService) override;

  virtual PHandlerServiceChild* AllocPHandlerServiceChild() override;

  virtual bool DeallocPHandlerServiceChild(PHandlerServiceChild*) override;

  virtual PCellBroadcastChild* AllocPCellBroadcastChild() override;

  PCellBroadcastChild* SendPCellBroadcastConstructor(PCellBroadcastChild* aActor);

  virtual bool DeallocPCellBroadcastChild(PCellBroadcastChild* aActor) override;

  virtual PSmsChild* AllocPSmsChild() override;

  virtual bool DeallocPSmsChild(PSmsChild*) override;

  virtual PTelephonyChild* AllocPTelephonyChild() override;

  virtual bool DeallocPTelephonyChild(PTelephonyChild*) override;

  virtual PVoicemailChild* AllocPVoicemailChild() override;

  PVoicemailChild* SendPVoicemailConstructor(PVoicemailChild* aActor);

  virtual bool DeallocPVoicemailChild(PVoicemailChild*) override;

  virtual PMediaChild* AllocPMediaChild() override;

  virtual bool DeallocPMediaChild(PMediaChild* aActor) override;

  virtual PStorageChild* AllocPStorageChild() override;

  virtual bool DeallocPStorageChild(PStorageChild* aActor) override;

  virtual PBluetoothChild* AllocPBluetoothChild() override;

  virtual bool DeallocPBluetoothChild(PBluetoothChild* aActor) override;

  virtual PFMRadioChild* AllocPFMRadioChild() override;

  virtual bool DeallocPFMRadioChild(PFMRadioChild* aActor) override;

  virtual PPresentationChild* AllocPPresentationChild() override;

  virtual bool DeallocPPresentationChild(PPresentationChild* aActor) override;

  virtual bool
  RecvNotifyPresentationReceiverLaunched(PBrowserChild* aIframe,
                                         const nsString& aSessionId) override;

  virtual bool
  RecvNotifyPresentationReceiverCleanUp(const nsString& aSessionId) override;

  virtual bool RecvNotifyGMPsChanged() override;

  virtual PSpeechSynthesisChild* AllocPSpeechSynthesisChild() override;

  virtual bool DeallocPSpeechSynthesisChild(PSpeechSynthesisChild* aActor) override;

  virtual bool RecvRegisterChrome(InfallibleTArray<ChromePackage>&& packages,
                                  InfallibleTArray<SubstitutionMapping>&& resources,
                                  InfallibleTArray<OverrideMapping>&& overrides,
                                  const nsCString& locale,
                                  const bool& reset) override;
  virtual bool RecvRegisterChromeItem(const ChromeRegistryItem& item) override;

  virtual mozilla::jsipc::PJavaScriptChild* AllocPJavaScriptChild() override;

  virtual bool DeallocPJavaScriptChild(mozilla::jsipc::PJavaScriptChild*) override;

  virtual PRemoteSpellcheckEngineChild* AllocPRemoteSpellcheckEngineChild() override;

  virtual bool DeallocPRemoteSpellcheckEngineChild(PRemoteSpellcheckEngineChild*) override;

  virtual bool RecvSetOffline(const bool& offline) override;

  virtual bool RecvSetConnectivity(const bool& connectivity) override;

  virtual bool RecvSpeakerManagerNotify() override;

  virtual bool RecvBidiKeyboardNotify(const bool& isLangRTL) override;

  virtual bool RecvNotifyVisited(const URIParams& aURI) override;

  // auto remove when alertfinished is received.
  nsresult AddRemoteAlertObserver(const nsString& aData, nsIObserver* aObserver);

  virtual bool RecvSystemMemoryAvailable(const uint64_t& aGetterId,
                                         const uint32_t& aMemoryAvailable) override;

  virtual bool RecvPreferenceUpdate(const PrefSetting& aPref) override;

  virtual bool RecvDataStoragePut(const nsString& aFilename,
                                  const DataStorageItem& aItem) override;

  virtual bool RecvDataStorageRemove(const nsString& aFilename,
                                     const nsCString& aKey,
                                     const DataStorageType& aType) override;

  virtual bool RecvDataStorageClear(const nsString& aFilename) override;

  virtual bool RecvNotifyAlertsObserver(const nsCString& aType,
                                        const nsString& aData) override;

  virtual bool RecvLoadProcessScript(const nsString& aURL) override;

  virtual bool RecvAsyncMessage(const nsString& aMsg,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal,
                                const ClonedMessageData& aData) override;

  virtual bool RecvGeolocationUpdate(const GeoPosition& somewhere) override;

  virtual bool RecvGeolocationError(const uint16_t& errorCode) override;

  virtual bool RecvUpdateDictionaryList(InfallibleTArray<nsString>&& aDictionaries) override;

  virtual bool RecvAddPermission(const IPC::Permission& permission) override;

  virtual bool RecvFlushMemory(const nsString& reason) override;

  virtual bool RecvActivateA11y() override;

  virtual bool RecvGarbageCollect() override;
  virtual bool RecvCycleCollect() override;

  virtual bool RecvAppInfo(const nsCString& version, const nsCString& buildID,
                           const nsCString& name, const nsCString& UAName,
                           const nsCString& ID, const nsCString& vendor) override;

  virtual bool RecvAppInit() override;

  virtual bool RecvLastPrivateDocShellDestroyed() override;

  virtual bool RecvVolumes(InfallibleTArray<VolumeInfo>&& aVolumes) override;

  virtual bool RecvFilePathUpdate(const nsString& aStorageType,
                                  const nsString& aStorageName,
                                  const nsString& aPath,
                                  const nsCString& aReason) override;

  virtual bool RecvFileSystemUpdate(const nsString& aFsName,
                                    const nsString& aVolumeName,
                                    const int32_t& aState,
                                    const int32_t& aMountGeneration,
                                    const bool& aIsMediaPresent,
                                    const bool& aIsSharing,
                                    const bool& aIsFormatting,
                                    const bool& aIsFake,
                                    const bool& aIsUnmounting,
                                    const bool& aIsRemovable,
                                    const bool& aIsHotSwappable) override;

  virtual bool RecvVolumeRemoved(const nsString& aFsName) override;

  virtual bool
  RecvNotifyProcessPriorityChanged(const hal::ProcessPriority& aPriority) override;

  virtual bool RecvMinimizeMemoryUsage() override;

  virtual bool RecvLoadAndRegisterSheet(const URIParams& aURI,
                                        const uint32_t& aType) override;

  virtual bool RecvUnregisterSheet(const URIParams& aURI,
                                   const uint32_t& aType) override;

  virtual bool RecvNotifyPhoneStateChange(const nsString& aState) override;

  void AddIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  void RemoveIdleObserver(nsIObserver* aObserver, uint32_t aIdleTimeInS);

  virtual bool RecvNotifyIdleObserver(const uint64_t& aObserver,
                                      const nsCString& aTopic,
                                      const nsString& aData) override;

  virtual bool RecvOnAppThemeChanged() override;

  virtual bool RecvAssociatePluginId(const uint32_t& aPluginId,
                                     const base::ProcessId& aProcessId) override;

  virtual bool RecvLoadPluginResult(const uint32_t& aPluginId,
                                    const bool& aResult) override;

  virtual bool RecvUpdateWindow(const uintptr_t& aChildId) override;

  virtual bool RecvStartProfiler(const ProfilerInitParams& params) override;

  virtual bool RecvPauseProfiler(const bool& aPause) override;

  virtual bool RecvStopProfiler() override;

  virtual bool RecvGatherProfile() override;

  virtual bool RecvDomainSetChanged(const uint32_t& aSetType,
                                    const uint32_t& aChangeType,
                                    const OptionalURIParams& aDomain) override;

  virtual bool RecvShutdown() override;

  virtual bool
  RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                        const uint32_t& aAction) override;

  virtual bool RecvEndDragSession(const bool& aDoneDrag,
                                  const bool& aUserCancelled,
                                  const mozilla::LayoutDeviceIntPoint& aEndDragPoint) override;

  virtual bool
  RecvPush(const nsCString& aScope,
           const IPC::Principal& aPrincipal,
           const nsString& aMessageId) override;

  virtual bool
  RecvPushWithData(const nsCString& aScope,
                   const IPC::Principal& aPrincipal,
                   const nsString& aMessageId,
                   InfallibleTArray<uint8_t>&& aData) override;

  virtual bool
  RecvPushSubscriptionChange(const nsCString& aScope,
                             const IPC::Principal& aPrincipal) override;

  virtual bool
  RecvPushError(const nsCString& aScope, const IPC::Principal& aPrincipal,
                const nsString& aMessage, const uint32_t& aFlags) override;

  virtual bool
  RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                              const IPC::Principal& aPrincipal) override;

  // Get the directory for IndexedDB files. We query the parent for this and
  // cache the value
  nsString &GetIndexedDBPath();

  ContentParentId GetID() const { return mID; }

  bool IsForApp() const { return mIsForApp; }
  bool IsForBrowser() const { return mIsForBrowser; }

  virtual PBlobChild*
  SendPBlobConstructor(PBlobChild* actor,
                       const BlobConstructorParams& params) override;

  virtual PFileDescriptorSetChild*
  AllocPFileDescriptorSetChild(const FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetChild(PFileDescriptorSetChild*) override;

  virtual bool SendPBrowserConstructor(PBrowserChild* actor,
                                       const TabId& aTabId,
                                       const IPCTabContext& context,
                                       const uint32_t& chromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) override;

  virtual bool RecvPBrowserConstructor(PBrowserChild* aCctor,
                                       const TabId& aTabId,
                                       const IPCTabContext& aContext,
                                       const uint32_t& aChromeFlags,
                                       const ContentParentId& aCpID,
                                       const bool& aIsForApp,
                                       const bool& aIsForBrowser) override;

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
                                      const TabId& aTabId) override;
  virtual bool
  DeallocPContentPermissionRequestChild(PContentPermissionRequestChild* actor) override;

  virtual bool RecvGamepadUpdate(const GamepadChangeEvent& aGamepadEvent) override;

  // Windows specific - set up audio session
  virtual bool
  RecvSetAudioSessionData(const nsID& aId,
                          const nsString& aDisplayName,
                          const nsString& aIconPath) override;

private:
  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);
  void StartForceKillTimer();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ProcessingError(Result aCode, const char* aReason) override;

  InfallibleTArray<nsAutoPtr<AlertObserver> > mAlertObservers;
  RefPtr<ConsoleListener> mConsoleListener;

  nsTHashtable<nsPtrHashKey<nsIObserver>> mIdleObservers;

  InfallibleTArray<nsString> mAvailableDictionaries;

  /**
   * An ID unique to the process containing our corresponding
   * content parent.
   *
   * We expect our content parent to set this ID immediately after opening a
   * channel to us.
   */
  ContentParentId mID;

  AppInfo mAppInfo;

  bool mIsForApp;
  bool mIsForBrowser;
  bool mCanOverrideProcessName;
  bool mIsAlive;
  nsString mProcessName;

  static ContentChild* sSingleton;

  nsCOMPtr<nsIDomainPolicy> mPolicy;
  nsCOMPtr<nsITimer> mForceKillTimer;

  DISALLOW_EVIL_CONSTRUCTORS(ContentChild);
};

void
InitOnContentProcessCreated();

uint64_t
NextWindowID();

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContentChild_h

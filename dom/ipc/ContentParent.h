/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentParent_h
#define mozilla_dom_ContentParent_h

#include "mozilla/dom/PContentParent.h"
#include "mozilla/dom/nsIContentParent.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/gfx/GPUProcessListener.h"
#include "mozilla/Attributes.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HalTypes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "ContentProcessHost.h"
#include "nsDataHashtable.h"
#include "nsPluginTags.h"
#include "nsFrameMessageManager.h"
#include "nsHashKeys.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "nsRefPtrHashtable.h"
#include "PermissionMessageUtils.h"
#include "DriverCrashGuard.h"

#define CHILD_PROCESS_SHUTDOWN_MESSAGE NS_LITERAL_STRING("child-process-shutdown")

// These must match the similar ones in E10SUtils.jsm.
// Process names as reported by about:memory are defined in
// ContentChild:RecvRemoteType.  Add your value there too or it will be called
// "Web Content".
#define DEFAULT_REMOTE_TYPE "web"
#define FILE_REMOTE_TYPE "file"
#define EXTENSION_REMOTE_TYPE "extension"
#define PRIVILEGED_REMOTE_TYPE "privileged"

// This must start with the DEFAULT_REMOTE_TYPE above.
#define LARGE_ALLOCATION_REMOTE_TYPE "webLargeAllocation"

class nsConsoleService;
class nsIContentProcessInfo;
class nsICycleCollectorLogSink;
class nsIDumpGCAndCCLogsCallback;
class nsITabParent;
class nsITimer;
class ParentIdleListener;
class nsIWidget;

namespace mozilla {
class PRemoteSpellcheckEngineParent;

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
class SandboxBroker;
class SandboxBrokerPolicyFactory;
#endif

class PreallocatedProcessManagerImpl;

using mozilla::loader::PScriptCacheParent;

namespace embedding {
class PrintingParent;
}

namespace ipc {
class CrashReporterHost;
class OptionalURIParams;
class PFileDescriptorSetParent;
class URIParams;
class TestShellParent;
#ifdef FUZZING
class ProtocolFuzzerHelper;
#endif
} // namespace ipc

namespace jsipc {
class PJavaScriptParent;
} // namespace jsipc

namespace layers {
struct TextureFactoryIdentifier;
} // namespace layers

namespace layout {
class PRenderFrameParent;
} // namespace layout

namespace dom {

class Element;
class TabParent;
class ClonedMessageData;
class MemoryReport;
class TabContext;
class ContentBridgeParent;
class GetFilesHelper;
class MemoryReportRequestHost;

class ContentParent final : public PContentParent
                          , public nsIContentParent
                          , public nsIObserver
                          , public nsIDOMGeoPositionCallback
                          , public nsIDOMGeoPositionErrorCallback
                          , public nsIInterfaceRequestor
                          , public gfx::gfxVarReceiver
                          , public mozilla::LinkedListElement<ContentParent>
                          , public gfx::GPUProcessListener
                          , public mozilla::MemoryReportingProcess
{
  typedef mozilla::ipc::OptionalURIParams OptionalURIParams;
  typedef mozilla::ipc::PFileDescriptorSetParent PFileDescriptorSetParent;
  typedef mozilla::ipc::TestShellParent TestShellParent;
  typedef mozilla::ipc::URIParams URIParams;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;

  friend class ContentProcessHost;
  friend class mozilla::PreallocatedProcessManagerImpl;
#ifdef FUZZING
  friend class mozilla::ipc::ProtocolFuzzerHelper;
#endif

public:

  virtual bool IsContentParent() const override { return true; }

  /**
   * Create a subprocess suitable for use later as a content process.
   */
  static already_AddRefed<ContentParent> PreallocateProcess();

  /**
   * Start up the content-process machinery.  This might include
   * scheduling pre-launch tasks.
   */
  static void StartUp();

  /** Shut down the content-process machinery. */
  static void ShutDown();

  static uint32_t GetPoolSize(const nsAString& aContentProcessType);

  static uint32_t GetMaxProcessCount(const nsAString& aContentProcessType);

  static bool IsMaxProcessCountReached(const nsAString& aContentProcessType);

  static void ReleaseCachedProcesses();

  /**
   * Picks a random content parent from |aContentParents| with a given |aOpener|
   * respecting the index limit set by |aMaxContentParents|.
   * Returns null if non available.
   */
  static already_AddRefed<ContentParent>
  MinTabSelect(const nsTArray<ContentParent*>& aContentParents,
               ContentParent* aOpener,
               int32_t maxContentParents);

  /**
   * Get or create a content process for:
   * 1. browser iframe
   * 2. remote xul <browser>
   * 3. normal iframe
   */
  static already_AddRefed<ContentParent>
  GetNewOrUsedBrowserProcess(Element* aFrameElement,
                             const nsAString& aRemoteType,
                             hal::ProcessPriority aPriority =
                             hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
                             ContentParent* aOpener = nullptr,
                             bool aPreferUsed = false);

  /**
   * Get or create a content process for a JS plugin. aPluginID is the id of the JS plugin
   * (@see nsFakePlugin::mId). There is a maximum of one process per JS plugin.
   */
  static already_AddRefed<ContentParent>
  GetNewOrUsedJSPluginProcess(uint32_t aPluginID,
                              const hal::ProcessPriority& aPriority);

  /**
   * Get or create a content process for the given TabContext.  aFrameElement
   * should be the frame/iframe element with which this process will
   * associated.
   */
  static TabParent*
  CreateBrowser(const TabContext& aContext,
                Element* aFrameElement,
                ContentParent* aOpenerContentParent,
                TabParent* aSameTabGroupAs,
                uint64_t aNextTabParentId);

  static void GetAll(nsTArray<ContentParent*>& aArray);

  static void GetAllEvenIfDead(nsTArray<ContentParent*>& aArray);

  static void BroadcastStringBundle(const StringBundleDescriptor&);

  const nsAString& GetRemoteType() const;

  virtual void DoGetRemoteType(nsAString& aRemoteType, ErrorResult& aError) const override
  {
    aRemoteType = GetRemoteType();
  }

  enum CPIteratorPolicy {
    eLive,
    eAll
  };

  class ContentParentIterator {
  private:
    ContentParent* mCurrent;
    CPIteratorPolicy mPolicy;

  public:
    ContentParentIterator(CPIteratorPolicy aPolicy, ContentParent* aCurrent)
      : mCurrent(aCurrent),
        mPolicy(aPolicy)
    {
    }

    ContentParentIterator begin()
    {
      // Move the cursor to the first element that matches the policy.
      while (mPolicy != eAll && mCurrent && !mCurrent->mIsAlive) {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      }

      return *this;
    }
    ContentParentIterator end()
    {
      return ContentParentIterator(mPolicy, nullptr);
    }

    const ContentParentIterator& operator++()
    {
      MOZ_ASSERT(mCurrent);
      do {
        mCurrent = mCurrent->LinkedListElement<ContentParent>::getNext();
      } while (mPolicy != eAll && mCurrent && !mCurrent->mIsAlive);

      return *this;
    }

    bool operator!=(const ContentParentIterator& aOther)
    {
      MOZ_ASSERT(mPolicy == aOther.mPolicy);
      return mCurrent != aOther.mCurrent;
    }

    ContentParent* operator*()
    {
      return mCurrent;
    }
  };

  static ContentParentIterator AllProcesses(CPIteratorPolicy aPolicy)
  {
    ContentParent* first =
      sContentParents ? sContentParents->getFirst() : nullptr;
    return ContentParentIterator(aPolicy, first);
  }

  static bool IgnoreIPCPrincipal();

  static void NotifyUpdatedDictionaries();

  static void NotifyUpdatedFonts();

#if defined(XP_WIN)
  /**
   * Windows helper for firing off an update window request to a plugin
   * instance.
   *
   * aWidget - the eWindowType_plugin_ipc_chrome widget associated with
   *           this plugin window.
   */
  static void SendAsyncUpdate(nsIWidget* aWidget);
#endif

  // Let managees query if it is safe to send messages.
  bool IsDestroyed() const { return !mIPCOpen; }

  virtual mozilla::ipc::IPCResult RecvCreateChildProcess(const IPCTabContext& aContext,
                                                         const hal::ProcessPriority& aPriority,
                                                         const TabId& aOpenerTabId,
                                                         const TabId& aTabId,
                                                         ContentParentId* aCpId,
                                                         bool* aIsForBrowser) override;

  virtual mozilla::ipc::IPCResult RecvBridgeToChildProcess(const ContentParentId& aCpId,
                                                           Endpoint<PContentBridgeParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvOpenRecordReplayChannel(const uint32_t& channelId,
                                                              FileDescriptor* connection) override;
  virtual mozilla::ipc::IPCResult RecvCreateReplayingProcess(const uint32_t& aChannelId) override;
  virtual mozilla::ipc::IPCResult RecvTerminateReplayingProcess(const uint32_t& aChannelId) override;

  virtual mozilla::ipc::IPCResult RecvCreateGMPService() override;

  virtual mozilla::ipc::IPCResult RecvLoadPlugin(const uint32_t& aPluginId, nsresult* aRv,
                                                 uint32_t* aRunID,
                                                 Endpoint<PPluginModuleParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvMaybeReloadPlugins() override;

  virtual mozilla::ipc::IPCResult RecvConnectPluginBridge(const uint32_t& aPluginId,
                                                          nsresult* aRv,
                                                          Endpoint<PPluginModuleParent>* aEndpoint) override;

  virtual mozilla::ipc::IPCResult RecvUngrabPointer(const uint32_t& aTime) override;

  virtual mozilla::ipc::IPCResult RecvRemovePermission(const IPC::Principal& aPrincipal,
                                                       const nsCString& aPermissionType,
                                                       nsresult* aRv) override;

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ContentParent, nsIObserver)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMGEOPOSITIONCALLBACK
  NS_DECL_NSIDOMGEOPOSITIONERRORCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) override;

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  /** Notify that a tab is beginning its destruction sequence. */
  static void NotifyTabDestroying(const TabId& aTabId,
                                  const ContentParentId& aCpId);

  /** Notify that a tab was destroyed during normal operation. */
  void NotifyTabDestroyed(const TabId& aTabId,
                          bool aNotifiedDestroying);

  TestShellParent* CreateTestShell();

  bool DestroyTestShell(TestShellParent* aTestShell);

  TestShellParent* GetTestShellSingleton();

  jsipc::CPOWManager* GetCPOWManager() override;

  static void
  UnregisterRemoteFrame(const TabId& aTabId,
                        const ContentParentId& aCpId,
                        bool aMarkedDestroying);

  void ReportChildAlreadyBlocked();

  bool RequestRunToCompletion();

  void UpdateCookieStatus(nsIChannel *aChannel);

  bool IsAvailable() const
  {
    return mIsAvailable;
  }
  bool IsAlive() const override;

  virtual bool IsForBrowser() const override
  {
    return mIsForBrowser;
  }
  virtual bool IsForJSPlugin() const override
  {
    return mJSPluginID != nsFakePluginTag::NOT_JSPLUGIN;
  }

  ContentProcessHost* Process() const
  {
    return mSubprocess;
  }

  ContentParent* Opener() const
  {
    return mOpener;
  }
  nsIContentProcessInfo* ScriptableHelper() const
  {
    return mScriptableHelper;
  }

  bool NeedsPermissionsUpdate(const nsACString& aPermissionKey) const;

  /**
   * Kill our subprocess and make sure it dies.  Should only be used
   * in emergency situations since it bypasses the normal shutdown
   * process.
   *
   * WARNING: aReason appears in telemetry, so any new value passed in requires
   * data review.
   */
  void KillHard(const char* aWhy);

  ContentParentId ChildID() const override { return mChildID; }

  /**
   * Get a user-friendly name for this ContentParent.  We make no guarantees
   * about this name: It might not be unique, apps can spoof special names,
   * etc.  So please don't use this name to make any decisions about the
   * ContentParent based on the value returned here.
   */
  void FriendlyName(nsAString& aName, bool aAnonymize = false);

  virtual void OnChannelError() override;

  virtual mozilla::ipc::IPCResult
  RecvInitCrashReporter(Shmem&& aShmem, const NativeThreadId& aThreadId) override;

  virtual PNeckoParent* AllocPNeckoParent() override;

  virtual mozilla::ipc::IPCResult RecvPNeckoConstructor(PNeckoParent* aActor) override
  {
    return PContentParent::RecvPNeckoConstructor(aActor);
  }

  virtual PPrintingParent* AllocPPrintingParent() override;

  virtual bool DeallocPPrintingParent(PPrintingParent* aActor) override;

#if defined(NS_PRINTING)
  /**
   * @return the PrintingParent for this ContentParent.
   */
  already_AddRefed<embedding::PrintingParent> GetPrintingParent();
#endif

  virtual mozilla::ipc::IPCResult
  RecvInitStreamFilter(const uint64_t& aChannelId,
                       const nsString& aAddonId,
                       InitStreamFilterResolver&& aResolver) override;

  virtual PChildToParentStreamParent* AllocPChildToParentStreamParent() override;
  virtual bool
  DeallocPChildToParentStreamParent(PChildToParentStreamParent* aActor) override;

  virtual PParentToChildStreamParent*
  SendPParentToChildStreamConstructor(PParentToChildStreamParent*) override;

  virtual PFileDescriptorSetParent*
  SendPFileDescriptorSetConstructor(const FileDescriptor&) override;

  virtual PParentToChildStreamParent* AllocPParentToChildStreamParent() override;
  virtual bool
  DeallocPParentToChildStreamParent(PParentToChildStreamParent* aActor) override;

  virtual PHalParent* AllocPHalParent() override;

  virtual mozilla::ipc::IPCResult RecvPHalConstructor(PHalParent* aActor) override
  {
    return PContentParent::RecvPHalConstructor(aActor);
  }

  virtual PHeapSnapshotTempFileHelperParent*
  AllocPHeapSnapshotTempFileHelperParent() override;

  virtual PJavaScriptParent*
  AllocPJavaScriptParent() override;

  virtual mozilla::ipc::IPCResult
  RecvPJavaScriptConstructor(PJavaScriptParent* aActor) override
  {
    return PContentParent::RecvPJavaScriptConstructor(aActor);
  }

  virtual PRemoteSpellcheckEngineParent* AllocPRemoteSpellcheckEngineParent() override;

  virtual mozilla::ipc::IPCResult RecvRecordingDeviceEvents(const nsString& aRecordingStatus,
                                                            const nsString& aPageURL,
                                                            const bool& aIsAudio,
                                                            const bool& aIsVideo) override;

  bool CycleCollectWithLogs(bool aDumpAllTraces,
                            nsICycleCollectorLogSink* aSink,
                            nsIDumpGCAndCCLogsCallback* aCallback);

  virtual mozilla::ipc::IPCResult RecvUnregisterRemoteFrame(const TabId& aTabId,
                                                            const ContentParentId& aCpId,
                                                            const bool& aMarkedDestroying) override;

  virtual mozilla::ipc::IPCResult RecvNotifyTabDestroying(const TabId& aTabId,
                                                          const ContentParentId& aCpId) override;

  nsTArray<TabContext> GetManagedTabContext();

  virtual POfflineCacheUpdateParent*
  AllocPOfflineCacheUpdateParent(const URIParams& aManifestURI,
                                 const URIParams& aDocumentURI,
                                 const PrincipalInfo& aLoadingPrincipalInfo,
                                 const bool& aStickDocument) override;

  virtual mozilla::ipc::IPCResult
  RecvPOfflineCacheUpdateConstructor(POfflineCacheUpdateParent* aActor,
                                     const URIParams& aManifestURI,
                                     const URIParams& aDocumentURI,
                                     const PrincipalInfo& aLoadingPrincipal,
                                     const bool& stickDocument) override;

  virtual bool
  DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvSetOfflinePermission(const IPC::Principal& principal) override;

  virtual mozilla::ipc::IPCResult RecvFinishShutdown() override;

  void MaybeInvokeDragSession(TabParent* aParent);

  virtual PContentPermissionRequestParent*
  AllocPContentPermissionRequestParent(const InfallibleTArray<PermissionRequest>& aRequests,
                                       const IPC::Principal& aPrincipal,
                                       const bool& aIsTrusted,
                                       const TabId& aTabId) override;

  virtual bool
  DeallocPContentPermissionRequestParent(PContentPermissionRequestParent* actor) override;

  virtual bool HandleWindowsMessages(const Message& aMsg) const override;

  void ForkNewProcess(bool aBlocking);

  virtual mozilla::ipc::IPCResult
  RecvCreateWindow(PBrowserParent* aThisTabParent,
                   PBrowserParent* aNewTab,
                   layout::PRenderFrameParent* aRenderFrame,
                   const uint32_t& aChromeFlags,
                   const bool& aCalledFromJS,
                   const bool& aPositionSpecified,
                   const bool& aSizeSpecified,
                   const OptionalURIParams& aURIToLoad,
                   const nsCString& aFeatures,
                   const nsCString& aBaseURI,
                   const float& aFullZoom,
                   const IPC::Principal& aTriggeringPrincipal,
                   const uint32_t& aReferrerPolicy,
                   CreateWindowResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult RecvCreateWindowInDifferentProcess(
    PBrowserParent* aThisTab,
    const uint32_t& aChromeFlags,
    const bool& aCalledFromJS,
    const bool& aPositionSpecified,
    const bool& aSizeSpecified,
    const OptionalURIParams& aURIToLoad,
    const nsCString& aFeatures,
    const nsCString& aBaseURI,
    const float& aFullZoom,
    const nsString& aName,
    const IPC::Principal& aTriggeringPrincipal,
    const uint32_t& aReferrerPolicy) override;

  static bool AllocateLayerTreeId(TabParent* aTabParent, layers::LayersId* aId);

  static void
  BroadcastBlobURLRegistration(const nsACString& aURI,
                               BlobImpl* aBlobImpl,
                               nsIPrincipal* aPrincipal,
                               ContentParent* aIgnoreThisCP = nullptr);

  static void
  BroadcastBlobURLUnregistration(const nsACString& aURI,
                                 ContentParent* aIgnoreThisCP = nullptr);

  virtual mozilla::ipc::IPCResult
  RecvStoreAndBroadcastBlobURLRegistration(const nsCString& aURI,
                                           const IPCBlob& aBlob,
                                           const Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult
  RecvUnstoreAndBroadcastBlobURLUnregistration(const nsCString& aURI) override;

  virtual mozilla::ipc::IPCResult
  RecvGetA11yContentId(uint32_t* aContentId) override;

  virtual mozilla::ipc::IPCResult
  RecvA11yHandlerControl(const uint32_t& aPid,
                         const IHandlerControlHolder& aHandlerControl) override;

  virtual int32_t Pid() const override;

  // PURLClassifierParent.
  virtual PURLClassifierParent*
  AllocPURLClassifierParent(const Principal& aPrincipal,
                            const bool& aUseTrackingProtection,
                            bool* aSuccess) override;
  virtual mozilla::ipc::IPCResult
  RecvPURLClassifierConstructor(PURLClassifierParent* aActor,
                                const Principal& aPrincipal,
                                const bool& aUseTrackingProtection,
                                bool* aSuccess) override;

  // PURLClassifierLocalParent.
  virtual PURLClassifierLocalParent*
  AllocPURLClassifierLocalParent(const URIParams& aURI,
                                 const nsCString& aTables) override;
  virtual mozilla::ipc::IPCResult
  RecvPURLClassifierLocalConstructor(PURLClassifierLocalParent* aActor,
                                     const URIParams& aURI,
                                     const nsCString& aTables) override;

  virtual PLoginReputationParent*
  AllocPLoginReputationParent(const URIParams& aURI) override;

  virtual mozilla::ipc::IPCResult
  RecvPLoginReputationConstructor(PLoginReputationParent* aActor,
                                  const URIParams& aURI) override;

  virtual bool
  DeallocPLoginReputationParent(PLoginReputationParent* aActor) override;

  virtual bool SendActivate(PBrowserParent* aTab) override
  {
    return PContentParent::SendActivate(aTab);
  }

  virtual bool SendDeactivate(PBrowserParent* aTab) override
  {
    return PContentParent::SendDeactivate(aTab);
  }

  virtual bool
  DeallocPURLClassifierLocalParent(PURLClassifierLocalParent* aActor) override;

  virtual bool
  DeallocPURLClassifierParent(PURLClassifierParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvClassifyLocal(const URIParams& aURI,
                    const nsCString& aTables,
                    nsresult* aRv,
                    nsTArray<nsCString>* aResults) override;

  // Use the PHangMonitor channel to ask the child to repaint a tab.
  void PaintTabWhileInterruptingJS(TabParent* aTabParent, bool aForceRepaint, uint64_t aLayerObserverEpoch);

  // This function is called when we are about to load a document from an
  // HTTP(S), FTP or wyciwyg channel for a content process.  It is a useful
  // place to start to kick off work as early as possible in response to such
  // document loads.
  nsresult AboutToLoadHttpFtpWyciwygDocumentForChild(nsIChannel* aChannel);

  nsresult TransmitPermissionsForPrincipal(nsIPrincipal* aPrincipal);

  void OnCompositorDeviceReset() override;

  virtual PClientOpenWindowOpParent*
  AllocPClientOpenWindowOpParent(const ClientOpenWindowArgs& aArgs) override;

  virtual bool
  DeallocPClientOpenWindowOpParent(PClientOpenWindowOpParent* aActor) override;

  static hal::ProcessPriority GetInitialProcessPriority(Element* aFrameElement);

  // Control the priority of the IPC messages for input events.
  void SetInputPriorityEventEnabled(bool aEnabled);
  bool IsInputPriorityEventEnabled()
  {
    return mIsInputPriorityEventEnabled;
  }

  static bool IsInputEventQueueSupported();

protected:
  void OnChannelConnected(int32_t pid) override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  bool ShouldContinueFromReplyTimeout() override;

  void OnVarChanged(const GfxVarUpdate& aVar) override;
  void OnCompositorUnexpectedShutdown() override;

private:
  /**
   * A map of the remote content process type to a list of content parents
   * currently available to host *new* tabs/frames of that type.
   *
   * If a content process is identified as troubled or dead, it will be
   * removed from this list, but will still be in the sContentParents list for
   * the GetAll/GetAllEvenIfDead APIs.
   */
  static nsClassHashtable<nsStringHashKey, nsTArray<ContentParent*>>* sBrowserContentParents;
  static nsTArray<ContentParent*>* sPrivateContent;
  static nsDataHashtable<nsUint32HashKey, ContentParent*> *sJSPluginContentParents;
  static StaticAutoPtr<LinkedList<ContentParent> > sContentParents;

  static ContentBridgeParent* CreateContentBridgeParent(const TabContext& aContext,
                                                        const hal::ProcessPriority& aPriority,
                                                        const TabId& aOpenerTabId,
                                                        const TabId& aTabId);

  // Hide the raw constructor methods since we don't want client code
  // using them.
  virtual PBrowserParent* SendPBrowserConstructor(
      PBrowserParent* actor,
      const TabId& aTabId,
      const TabId& aSameTabGroupsAs,
      const IPCTabContext& context,
      const uint32_t& chromeFlags,
      const ContentParentId& aCpId,
      const bool& aIsForBrowser) override;
  using PContentParent::SendPTestShellConstructor;

  // Set aLoadUri to true to load aURIToLoad and to false to only create the
  // window. aURIToLoad should always be provided, if available, to ensure
  // compatibility with GeckoView.
  mozilla::ipc::IPCResult
  CommonCreateWindow(PBrowserParent* aThisTab,
                     bool aSetOpener,
                     const uint32_t& aChromeFlags,
                     const bool& aCalledFromJS,
                     const bool& aPositionSpecified,
                     const bool& aSizeSpecified,
                     nsIURI* aURIToLoad,
                     const nsCString& aFeatures,
                     const nsCString& aBaseURI,
                     const float& aFullZoom,
                     uint64_t aNextTabParentId,
                     const nsString& aName,
                     nsresult& aResult,
                     nsCOMPtr<nsITabParent>& aNewTabParent,
                     bool* aWindowIsNew,
                     int32_t& aOpenLocation,
                     nsIPrincipal* aTriggeringPrincipal,
                     uint32_t aReferrerPolicy,
                     bool aLoadUri);

  FORWARD_SHMEM_ALLOCATOR_TO(PContentParent)

  enum RecordReplayState
  {
    eNotRecordingOrReplaying,
    eRecording,
    eReplaying
  };

  explicit ContentParent(int32_t aPluginID)
    : ContentParent(nullptr, EmptyString(), eNotRecordingOrReplaying, EmptyString(), aPluginID)
  {}
  ContentParent(ContentParent* aOpener,
                const nsAString& aRemoteType,
                RecordReplayState aRecordReplayState = eNotRecordingOrReplaying,
                const nsAString& aRecordingFile = EmptyString())
    : ContentParent(aOpener, aRemoteType, aRecordReplayState, aRecordingFile,
                    nsFakePluginTag::NOT_JSPLUGIN)
  {}

  ContentParent(ContentParent* aOpener,
                const nsAString& aRemoteType,
                RecordReplayState aRecordReplayState,
                const nsAString& aRecordingFile,
                int32_t aPluginID);

  // Launch the subprocess and associated initialization.
  // Returns false if the process fails to start.
  bool LaunchSubprocess(hal::ProcessPriority aInitialPriority = hal::PROCESS_PRIORITY_FOREGROUND);

  // Common initialization after sub process launch.
  void InitInternal(ProcessPriority aPriority);

  virtual ~ContentParent();

  void Init();

  // Some information could be sent to content very early, it
  // should be send from this function. This function should only be
  // called after the process has been transformed to browser.
  void ForwardKnownInfo();

  /**
   * We might want to reuse barely used content processes if certain criteria are met.
   */
  bool TryToRecycle();

  /**
   * Removing it from the static array so it won't be returned for new tabs in
   * GetNewOrUsedBrowserProcess.
   */
  void RemoveFromList();

  /**
   * Decide whether the process should be kept alive even when it would normally
   * be shut down, for example when all its tabs are closed.
   */
  bool ShouldKeepProcessAlive() const;

  /**
   * Mark this ContentParent as "troubled". This means that it is still alive,
   * but it won't be returned for new tabs in GetNewOrUsedBrowserProcess.
   */
  void MarkAsTroubled();

  /**
   * Mark this ContentParent as dead for the purposes of Get*().
   * This method is idempotent.
   */
  void MarkAsDead();

  /**
   * How we will shut down this ContentParent and its subprocess.
   */
  enum ShutDownMethod
  {
    // Send a shutdown message and wait for FinishShutdown call back.
    SEND_SHUTDOWN_MESSAGE,
    // Close the channel ourselves and let the subprocess clean up itself.
    CLOSE_CHANNEL,
    // Close the channel with error and let the subprocess clean up itself.
    CLOSE_CHANNEL_WITH_ERROR,
  };

  /**
   * Exit the subprocess and vamoose.  After this call IsAlive()
   * will return false and this ContentParent will not be returned
   * by the Get*() funtions.  However, the shutdown sequence itself
   * may be asynchronous.
   *
   * If aMethod is CLOSE_CHANNEL_WITH_ERROR and this is the first call
   * to ShutDownProcess, then we'll close our channel using CloseWithError()
   * rather than vanilla Close().  CloseWithError() indicates to IPC that this
   * is an abnormal shutdown (e.g. a crash).
   */
  void ShutDownProcess(ShutDownMethod aMethod);

  // Perform any steps necesssary to gracefully shtudown the message
  // manager and null out mMessageManager.
  void ShutDownMessageManager();

  // Start the force-kill timer on shutdown.
  void StartForceKillTimer();

  void OnGenerateMinidumpComplete(bool aDumpResult);

  // Ensure that the permissions for the giben Permission key are set in the
  // content process.
  //
  // See nsIPermissionManager::GetPermissionsForKey for more information on
  // these keys.
  void EnsurePermissionsByKey(const nsCString& aKey);

  static void ForceKillTimerCallback(nsITimer* aTimer, void* aClosure);

  static bool AllocateLayerTreeId(ContentParent* aContent,
                                  TabParent* aTopLevel, const TabId& aTabId,
                                  layers::LayersId* aId);

  /**
   * Get or create the corresponding content parent array to |aContentProcessType|.
   */
  static nsTArray<ContentParent*>& GetOrCreatePool(const nsAString& aContentProcessType);

  virtual mozilla::ipc::IPCResult RecvInitBackground(Endpoint<mozilla::ipc::PBackgroundParent>&& aEndpoint) override;

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport) override;
  mozilla::ipc::IPCResult RecvFinishMemoryReport(const uint32_t& aGeneration) override;
  mozilla::ipc::IPCResult RecvAddPerformanceMetrics(const nsID& aID, nsTArray<PerformanceInfo>&& aMetrics) override;

  virtual bool
  DeallocPJavaScriptParent(mozilla::jsipc::PJavaScriptParent*) override;

  virtual bool
  DeallocPRemoteSpellcheckEngineParent(PRemoteSpellcheckEngineParent*) override;

  virtual PBrowserParent* AllocPBrowserParent(const TabId& aTabId,
                                              const TabId& aSameTabGroupAs,
                                              const IPCTabContext& aContext,
                                              const uint32_t& aChromeFlags,
                                              const ContentParentId& aCpId,
                                              const bool& aIsForBrowser) override;

  virtual bool DeallocPBrowserParent(PBrowserParent* frame) override;

  virtual mozilla::ipc::IPCResult
  RecvPBrowserConstructor(PBrowserParent* actor,
                          const TabId& tabId,
                          const TabId& sameTabGroupAs,
                          const IPCTabContext& context,
                          const uint32_t& chromeFlags,
                          const ContentParentId& cpId,
                          const bool& isForBrowser) override;

  virtual PIPCBlobInputStreamParent*
  SendPIPCBlobInputStreamConstructor(PIPCBlobInputStreamParent* aActor,
                                     const nsID& aID,
                                     const uint64_t& aSize) override;

  virtual PIPCBlobInputStreamParent*
  AllocPIPCBlobInputStreamParent(const nsID& aID,
                                 const uint64_t& aSize) override;

  virtual bool
  DeallocPIPCBlobInputStreamParent(PIPCBlobInputStreamParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvIsSecureURI(const uint32_t& aType, const URIParams& aURI,
                                                  const uint32_t& aFlags,
                                                  const OriginAttributes& aOriginAttributes,
                                                  bool* aIsSecureURI) override;

  virtual mozilla::ipc::IPCResult RecvAccumulateMixedContentHSTS(const URIParams& aURI,
                                                                 const bool& aActive,
                                                                 const OriginAttributes& aOriginAttributes) override;

  virtual bool DeallocPHalParent(PHalParent*) override;

  virtual bool
  DeallocPHeapSnapshotTempFileHelperParent(PHeapSnapshotTempFileHelperParent*) override;

  virtual PCycleCollectWithLogsParent*
  AllocPCycleCollectWithLogsParent(const bool& aDumpAllTraces,
                                   const FileDescriptor& aGCLog,
                                   const FileDescriptor& aCCLog) override;

  virtual bool
  DeallocPCycleCollectWithLogsParent(PCycleCollectWithLogsParent* aActor) override;

  virtual PTestShellParent* AllocPTestShellParent() override;

  virtual bool DeallocPTestShellParent(PTestShellParent* shell) override;

  virtual PScriptCacheParent*
  AllocPScriptCacheParent(const FileDescOrError& cacheFile,
                          const bool& wantCacheData) override;

  virtual bool DeallocPScriptCacheParent(PScriptCacheParent* shell) override;

  virtual bool DeallocPNeckoParent(PNeckoParent* necko) override;

  virtual PPSMContentDownloaderParent*
  AllocPPSMContentDownloaderParent(const uint32_t& aCertType) override;

  virtual bool
  DeallocPPSMContentDownloaderParent(PPSMContentDownloaderParent* aDownloader) override;

  virtual PExternalHelperAppParent*
  AllocPExternalHelperAppParent(const OptionalURIParams& aUri,
                                const nsCString& aMimeContentType,
                                const nsCString& aContentDisposition,
                                const uint32_t& aContentDispositionHint,
                                const nsString& aContentDispositionFilename,
                                const bool& aForceSave,
                                const int64_t& aContentLength,
                                const bool& aWasFileChannel,
                                const OptionalURIParams& aReferrer,
                                PBrowserParent* aBrowser) override;

  virtual bool
  DeallocPExternalHelperAppParent(PExternalHelperAppParent* aService) override;

  virtual PHandlerServiceParent* AllocPHandlerServiceParent() override;

  virtual bool DeallocPHandlerServiceParent(PHandlerServiceParent*) override;

  virtual PMediaParent* AllocPMediaParent() override;

  virtual bool DeallocPMediaParent(PMediaParent* aActor) override;

  virtual PPresentationParent* AllocPPresentationParent() override;

  virtual bool DeallocPPresentationParent(PPresentationParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvPPresentationConstructor(PPresentationParent* aActor) override;

  virtual PSpeechSynthesisParent* AllocPSpeechSynthesisParent() override;

  virtual bool
  DeallocPSpeechSynthesisParent(PSpeechSynthesisParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvPSpeechSynthesisConstructor(PSpeechSynthesisParent* aActor) override;

  virtual PWebBrowserPersistDocumentParent*
  AllocPWebBrowserPersistDocumentParent(PBrowserParent* aBrowser,
                                        const uint64_t& aOuterWindowID) override;

  virtual bool
  DeallocPWebBrowserPersistDocumentParent(PWebBrowserPersistDocumentParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvGetGfxVars(InfallibleTArray<GfxVarUpdate>* aVars) override;

  virtual mozilla::ipc::IPCResult RecvReadFontList(InfallibleTArray<FontListEntry>* retValue) override;

  virtual mozilla::ipc::IPCResult RecvSetClipboard(const IPCDataTransfer& aDataTransfer,
                                                   const bool& aIsPrivateData,
                                                   const IPC::Principal& aRequestingPrincipal,
                                                   const uint32_t& aContentPolicyType,
                                                   const int32_t& aWhichClipboard) override;

  virtual mozilla::ipc::IPCResult RecvGetClipboard(nsTArray<nsCString>&& aTypes,
                                                   const int32_t& aWhichClipboard,
                                                   IPCDataTransfer* aDataTransfer) override;

  virtual mozilla::ipc::IPCResult RecvEmptyClipboard(const int32_t& aWhichClipboard) override;

  virtual mozilla::ipc::IPCResult RecvClipboardHasType(nsTArray<nsCString>&& aTypes,
                                                       const int32_t& aWhichClipboard,
                                                       bool* aHasType) override;

  virtual mozilla::ipc::IPCResult RecvGetExternalClipboardFormats(const int32_t& aWhichClipboard,
                                                                  const bool& aPlainTextOnly,
                                                                  nsTArray<nsCString>* aTypes) override;

  virtual mozilla::ipc::IPCResult RecvPlaySound(const URIParams& aURI) override;
  virtual mozilla::ipc::IPCResult RecvBeep() override;
  virtual mozilla::ipc::IPCResult RecvPlayEventSound(const uint32_t& aEventId) override;

  virtual mozilla::ipc::IPCResult RecvGetSystemColors(const uint32_t& colorsCount,
                                                      InfallibleTArray<uint32_t>* colors) override;

  virtual mozilla::ipc::IPCResult RecvGetIconForExtension(const nsCString& aFileExt,
                                                          const uint32_t& aIconSize,
                                                          InfallibleTArray<uint8_t>* bits) override;

  virtual mozilla::ipc::IPCResult RecvGetShowPasswordSetting(bool* showPassword) override;

  virtual mozilla::ipc::IPCResult RecvStartVisitedQuery(const URIParams& uri) override;

  virtual mozilla::ipc::IPCResult RecvVisitURI(const URIParams& uri,
                                               const OptionalURIParams& referrer,
                                               const uint32_t& flags) override;

  virtual mozilla::ipc::IPCResult RecvSetURITitle(const URIParams& uri,
                                                  const nsString& title) override;

  bool HasNotificationPermission(const IPC::Principal& aPrincipal);

  virtual mozilla::ipc::IPCResult RecvShowAlert(nsIAlertNotification* aAlert) override;

  virtual mozilla::ipc::IPCResult RecvCloseAlert(const nsString& aName,
                                                 const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvDisableNotifications(const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvOpenNotificationSettings(const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvLoadURIExternal(const URIParams& uri,
                                                      PBrowserParent* windowContext) override;
  virtual mozilla::ipc::IPCResult RecvExtProtocolChannelConnectParent(const uint32_t& registrarId) override;

  virtual mozilla::ipc::IPCResult RecvSyncMessage(const nsString& aMsg,
                                                  const ClonedMessageData& aData,
                                                  InfallibleTArray<CpowEntry>&& aCpows,
                                                  const IPC::Principal& aPrincipal,
                                                  nsTArray<StructuredCloneData>* aRetvals) override;

  virtual mozilla::ipc::IPCResult RecvRpcMessage(const nsString& aMsg,
                                                 const ClonedMessageData& aData,
                                                 InfallibleTArray<CpowEntry>&& aCpows,
                                                 const IPC::Principal& aPrincipal,
                                                 nsTArray<StructuredCloneData>* aRetvals) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMsg,
                                                   InfallibleTArray<CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  virtual mozilla::ipc::IPCResult RecvAddGeolocationListener(const IPC::Principal& aPrincipal,
                                                             const bool& aHighAccuracy) override;
  virtual mozilla::ipc::IPCResult RecvRemoveGeolocationListener() override;

  virtual mozilla::ipc::IPCResult RecvSetGeolocationHigherAccuracy(const bool& aEnable) override;

  virtual mozilla::ipc::IPCResult RecvConsoleMessage(const nsString& aMessage) override;

  virtual mozilla::ipc::IPCResult RecvScriptError(const nsString& aMessage,
                                                  const nsString& aSourceName,
                                                  const nsString& aSourceLine,
                                                  const uint32_t& aLineNumber,
                                                  const uint32_t& aColNumber,
                                                  const uint32_t& aFlags,
                                                  const nsCString& aCategory,
                                                  const bool& aIsFromPrivateWindow) override;

  virtual mozilla::ipc::IPCResult RecvScriptErrorWithStack(const nsString& aMessage,
                                                           const nsString& aSourceName,
                                                           const nsString& aSourceLine,
                                                           const uint32_t& aLineNumber,
                                                           const uint32_t& aColNumber,
                                                           const uint32_t& aFlags,
                                                           const nsCString& aCategory,
                                                           const bool& aIsFromPrivateWindow,
                                                           const ClonedMessageData& aStack) override;

private:
  mozilla::ipc::IPCResult RecvScriptErrorInternal(const nsString& aMessage,
                                                  const nsString& aSourceName,
                                                  const nsString& aSourceLine,
                                                  const uint32_t& aLineNumber,
                                                  const uint32_t& aColNumber,
                                                  const uint32_t& aFlags,
                                                  const nsCString& aCategory,
                                                  const bool& aIsFromPrivateWindow,
                                                  const ClonedMessageData* aStack = nullptr);

public:
  virtual mozilla::ipc::IPCResult RecvPrivateDocShellsExist(const bool& aExist) override;

  virtual mozilla::ipc::IPCResult RecvFirstIdle() override;

  virtual mozilla::ipc::IPCResult RecvDeviceReset() override;

  virtual mozilla::ipc::IPCResult RecvKeywordToURI(const nsCString& aKeyword,
                                                   nsString* aProviderName,
                                                   RefPtr<nsIInputStream>* aPostData,
                                                   OptionalURIParams* aURI) override;

  virtual mozilla::ipc::IPCResult RecvNotifyKeywordSearchLoading(const nsString &aProvider,
                                                                 const nsString &aKeyword) override;

  virtual mozilla::ipc::IPCResult RecvCopyFavicon(const URIParams& aOldURI,
                                                  const URIParams& aNewURI,
                                                  const IPC::Principal& aLoadingPrincipal,
                                                  const bool& aInPrivateBrowsing) override;

  virtual void ProcessingError(Result aCode, const char* aMsgName) override;

  virtual mozilla::ipc::IPCResult RecvAllocateLayerTreeId(const ContentParentId& aCpId,
                                                          const TabId& aTabId,
                                                          layers::LayersId* aId) override;

  virtual mozilla::ipc::IPCResult RecvDeallocateLayerTreeId(const ContentParentId& aCpId,
                                                            const layers::LayersId& aId) override;

  virtual mozilla::ipc::IPCResult RecvGraphicsError(const nsCString& aError) override;

  virtual mozilla::ipc::IPCResult RecvRecordReplayFatalError(const nsCString& aError) override;

  virtual mozilla::ipc::IPCResult
  RecvBeginDriverCrashGuard(const uint32_t& aGuardType,
                            bool* aOutCrashed) override;

  virtual mozilla::ipc::IPCResult RecvEndDriverCrashGuard(const uint32_t& aGuardType) override;

  virtual mozilla::ipc::IPCResult RecvAddIdleObserver(const uint64_t& observerId,
                                                      const uint32_t& aIdleTimeInS) override;

  virtual mozilla::ipc::IPCResult RecvRemoveIdleObserver(const uint64_t& observerId,
                                                         const uint32_t& aIdleTimeInS) override;

  virtual mozilla::ipc::IPCResult
  RecvBackUpXResources(const FileDescriptor& aXSocketFd) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestAnonymousTemporaryFile(const uint64_t& aID) override;

  virtual mozilla::ipc::IPCResult
  RecvCreateAudioIPCConnection(CreateAudioIPCConnectionResolver&& aResolver) override;

  virtual mozilla::ipc::IPCResult
  RecvKeygenProcessValue(const nsString& oldValue, const nsString& challenge,
                         const nsString& keytype, const nsString& keyparams,
                         nsString* newValue) override;

  virtual mozilla::ipc::IPCResult
  RecvKeygenProvideContent(nsString* aAttribute,
                           nsTArray<nsString>* aContent) override;

  virtual PFileDescriptorSetParent*
  AllocPFileDescriptorSetParent(const mozilla::ipc::FileDescriptor&) override;

  virtual bool
  DeallocPFileDescriptorSetParent(PFileDescriptorSetParent*) override;

  virtual PWebrtcGlobalParent* AllocPWebrtcGlobalParent() override;
  virtual bool DeallocPWebrtcGlobalParent(PWebrtcGlobalParent *aActor) override;


  virtual mozilla::ipc::IPCResult RecvUpdateDropEffect(const uint32_t& aDragAction,
                                                       const uint32_t& aDropEffect) override;

  virtual mozilla::ipc::IPCResult RecvShutdownProfile(const nsCString& aProfile) override;

  virtual mozilla::ipc::IPCResult RecvGetGraphicsDeviceInitData(ContentDeviceData* aOut) override;

  virtual mozilla::ipc::IPCResult RecvGetAndroidSystemInfo(AndroidSystemInfo* aInfo) override;

  virtual mozilla::ipc::IPCResult RecvNotifyBenchmarkResult(const nsString& aCodecName,
                                                            const uint32_t& aDecodeFPS) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushObservers(const nsCString& aScope,
                                                          const IPC::Principal& aPrincipal,
                                                          const nsString& aMessageId) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushObserversWithData(const nsCString& aScope,
                                                                  const IPC::Principal& aPrincipal,
                                                                  const nsString& aMessageId,
                                                                  InfallibleTArray<uint8_t>&& aData) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushSubscriptionChangeObservers(const nsCString& aScope,
                                                                            const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvNotifyPushSubscriptionModifiedObservers(const nsCString& aScope,
                                                                              const IPC::Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult RecvNotifyLowMemory() override;

  virtual mozilla::ipc::IPCResult RecvGetFilesRequest(const nsID& aID,
                                                      const nsString& aDirectoryPath,
                                                      const bool& aRecursiveFlag) override;

  virtual mozilla::ipc::IPCResult RecvDeleteGetFilesRequest(const nsID& aID) override;

  virtual mozilla::ipc::IPCResult
  RecvFileCreationRequest(const nsID& aID, const nsString& aFullPath,
                          const nsString& aType, const nsString& aName,
                          const bool& aLastModifiedPassed,
                          const int64_t& aLastModified,
                          const bool& aExistenceCheck,
                          const bool& aIsFromNsIFile) override;

  virtual mozilla::ipc::IPCResult RecvAccumulateChildHistograms(
    InfallibleTArray<HistogramAccumulation>&& aAccumulations) override;
  virtual mozilla::ipc::IPCResult RecvAccumulateChildKeyedHistograms(
    InfallibleTArray<KeyedHistogramAccumulation>&& aAccumulations) override;
  virtual mozilla::ipc::IPCResult RecvUpdateChildScalars(
    InfallibleTArray<ScalarAction>&& aScalarActions) override;
  virtual mozilla::ipc::IPCResult RecvUpdateChildKeyedScalars(
    InfallibleTArray<KeyedScalarAction>&& aScalarActions) override;
  virtual mozilla::ipc::IPCResult RecvRecordChildEvents(
    nsTArray<ChildEventData>&& events) override;
  virtual mozilla::ipc::IPCResult RecvRecordDiscardedData(
    const DiscardedData& aDiscardedData) override;

  virtual mozilla::ipc::IPCResult RecvBHRThreadHang(
    const HangDetails& aHangDetails) override;

  virtual mozilla::ipc::IPCResult
  RecvFirstPartyStorageAccessGrantedForOrigin(const Principal& aParentPrincipal,
                                              const nsCString& aTrackingOrigin,
                                              const nsCString& aGrantedOrigin,
                                              FirstPartyStorageAccessGrantedForOriginResolver&& aResolver) override;

  // Notify the ContentChild to enable the input event prioritization when
  // initializing.
  void MaybeEnableRemoteInputEventQueue();

public:
  void SendGetFilesResponseAndForget(const nsID& aID,
                                     const GetFilesResponseResult& aResult);

  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const MaybeFileDesc& aDMDFile) override;

  bool CanCommunicateWith(ContentParentId aOtherProcess);

  nsresult SaveRecording(nsIFile* aFile, bool* aRetval);

  bool IsRecordingOrReplaying() const {
    return mRecordReplayState != eNotRecordingOrReplaying;
  }

private:

  // If you add strong pointers to cycle collected objects here, be sure to
  // release these objects in ShutDownProcess.  See the comment there for more
  // details.

  ContentProcessHost* mSubprocess;
  const TimeStamp mLaunchTS; // used to calculate time to start content process
  TimeStamp mActivateTS;
  ContentParent* mOpener;

  nsString mRemoteType;

  ContentParentId mChildID;
  int32_t mGeolocationWatchID;

  // This contains the id for the JS plugin (@see nsFakePluginTag) if this is the
  // ContentParent for a process containing iframes for that JS plugin.
  // If this is not a ContentParent for a JS plugin then it contains the value
  // nsFakePluginTag::NOT_JSPLUGIN.
  int32_t mJSPluginID;

  nsCString mKillHardAnnotation;

  // After we initiate shutdown, we also start a timer to ensure
  // that even content processes that are 100% blocked (say from
  // SIGSTOP), are still killed eventually.  This task enforces that
  // timer.
  nsCOMPtr<nsITimer> mForceKillTimer;
  // How many tabs we're waiting to finish their destruction
  // sequence.  Precisely, how many TabParents have called
  // NotifyTabDestroying() but not called NotifyTabDestroyed().
  int32_t mNumDestroyingTabs;
  // True only while this process is in "good health" and may be used for
  // new remote tabs.
  bool mIsAvailable;
  // True only while remote content is being actively used from this process.
  // After mIsAlive goes to false, some previously scheduled IPC traffic may
  // still pass through.
  bool mIsAlive;

  bool mIsForBrowser;

  // Whether this process is recording or replaying its execution, and any
  // associated recording file.
  RecordReplayState mRecordReplayState;
  nsString mRecordingFile;

  // When recording or replaying, the child process is a middleman. This vector
  // stores any replaying children we have spawned on behalf of that middleman,
  // indexed by their record/replay channel ID.
  Vector<mozilla::ipc::GeckoChildProcessHost*> mReplayingChildren;

  // These variables track whether we've called Close() and KillHard() on our
  // channel.
  bool mCalledClose;
  bool mCalledKillHard;
  bool mCreatedPairedMinidumps;
  bool mShutdownPending;
  bool mIPCOpen;

  // True if the input event queue on the main thread of the content process is
  // enabled.
  bool mIsRemoteInputEventQueueEnabled;

  // True if we send input events with input priority. Otherwise, we send input
  // events with normal priority.
  bool mIsInputPriorityEventEnabled;

  RefPtr<nsConsoleService>  mConsoleService;
  nsConsoleService* GetConsoleService();
  nsCOMPtr<nsIContentProcessInfo> mScriptableHelper;

  nsTArray<nsCOMPtr<nsIObserver>> mIdleListeners;

#ifdef MOZ_X11
  // Dup of child's X socket, used to scope its resources to this
  // object instead of the child process's lifetime.
  ScopedClose mChildXSocketFdDup;
#endif

  PProcessHangMonitorParent* mHangMonitorActor;

  UniquePtr<gfx::DriverCrashGuard> mDriverCrashGuard;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;

#if defined(XP_LINUX) && defined(MOZ_CONTENT_SANDBOX)
  mozilla::UniquePtr<SandboxBroker> mSandboxBroker;
  static mozilla::UniquePtr<SandboxBrokerPolicyFactory>
      sSandboxBrokerPolicyFactory;
#endif

#ifdef NS_PRINTING
  RefPtr<embedding::PrintingParent> mPrintingParent;
#endif

  // This hashtable is used to run GetFilesHelper objects in the parent process.
  // GetFilesHelper can be aborted by receiving RecvDeleteGetFilesRequest.
  nsRefPtrHashtable<nsIDHashKey, GetFilesHelper> mGetFilesPendingRequests;

  nsTHashtable<nsCStringHashKey> mActivePermissionKeys;

  nsTArray<nsCString> mBlobURLs;

  UniquePtr<mozilla::ipc::CrashReporterHost> mCrashReporter;

  static uint64_t sNextTabParentId;
  static nsDataHashtable<nsUint64HashKey, TabParent*> sNextTabParents;
};

} // namespace dom
} // namespace mozilla

class ParentIdleListener : public nsIObserver
{
  friend class mozilla::dom::ContentParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ParentIdleListener(mozilla::dom::ContentParent* aParent,
                     uint64_t aObserver, uint32_t aTime)
    : mParent(aParent), mObserver(aObserver), mTime(aTime)
  {}

private:
  virtual ~ParentIdleListener() {}

  RefPtr<mozilla::dom::ContentParent> mParent;
  uint64_t mObserver;
  uint32_t mTime;
};

#endif // mozilla_dom_ContentParent_h

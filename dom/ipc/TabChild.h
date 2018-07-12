/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabChild_h
#define mozilla_dom_TabChild_h

#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/PBrowserChild.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIDOMEventListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWindowProvider.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsFrameMessageManager.h"
#include "nsIPresShell.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsWeakReference.h"
#include "nsITabChild.h"
#include "nsITooltipListener.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/CoalescedMouseData.h"
#include "mozilla/dom/CoalescedWheelData.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/CompositorOptions.h"
#include "nsIWebBrowserChrome3.h"
#include "mozilla/dom/ipc/IdType.h"
#include "AudioChannelService.h"
#include "PuppetWidget.h"
#include "mozilla/layers/GeckoContentController.h"
#include "nsDeque.h"
#include "nsISHistoryListener.h"

class nsIDOMWindowUtils;
class nsIHttpChannel;
class nsISerialEventTarget;

template<typename T> class nsTHashtable;
template<typename T> class nsPtrHashKey;

namespace mozilla {
class AbstractThread;
namespace layout {
class RenderFrameChild;
} // namespace layout

namespace layers {
class APZChild;
class APZEventState;
class AsyncDragMetrics;
class IAPZCTreeManager;
class ImageCompositeNotification;
class PCompositorBridgeChild;
} // namespace layers

namespace widget {
struct AutoCacheNativeKeyCommands;
} // namespace widget

namespace dom {

class TabChild;
class TabGroup;
class ClonedMessageData;
class CoalescedMouseData;
class CoalescedWheelData;

class TabChildGlobal : public ContentFrameMessageManager,
                       public nsIContentFrameMessageManager,
                       public nsIScriptObjectPrincipal,
                       public nsIGlobalObject,
                       public nsSupportsWeakReference
{
public:
  explicit TabChildGlobal(TabChild* aTabChild);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TabChildGlobal, DOMEventTargetHelper)

  void MarkForCC();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("We should never get here!");
  }
  bool WrapGlobalObject(JSContext* aCx,
                        JS::RealmOptions& aOptions,
                        JS::MutableHandle<JSObject*> aReflector);

  virtual already_AddRefed<nsPIDOMWindowOuter> GetContent(ErrorResult& aError) override;
  virtual already_AddRefed<nsIDocShell> GetDocShell(ErrorResult& aError) override;
  virtual already_AddRefed<nsIEventTarget> GetTabEventTarget() override;
  virtual uint64_t ChromeOuterWindowID() override;

  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_DECL_NSICONTENTFRAMEMESSAGEMANAGER

  void
  GetEventTargetParent(EventChainPreVisitor& aVisitor) override
  {
    aVisitor.mForceContentDispatch = true;
  }

  virtual nsIPrincipal* GetPrincipal() override;
  virtual JSObject* GetGlobalJSObject() override;

  // Dispatch a runnable related to the global.
  virtual nsresult Dispatch(mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget*
  EventTargetFor(mozilla::TaskCategory aCategory) const override;

  virtual AbstractThread*
  AbstractMainThreadFor(mozilla::TaskCategory aCategory) override;

  RefPtr<TabChild> mTabChild;

protected:
  ~TabChildGlobal();
};

class ContentListener final : public nsIDOMEventListener
{
public:
  explicit ContentListener(TabChild* aTabChild) : mTabChild(aTabChild) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
protected:
  ~ContentListener() {}
  TabChild* mTabChild;
};

// This is base clase which helps to share Viewport and touch related
// functionality between b2g/android FF/embedlite clients implementation.
// It make sense to place in this class all helper functions, and functionality
// which could be shared between Cross-process/Cross-thread implmentations.
class TabChildBase : public nsISupports,
                     public nsMessageManagerScriptExecutor,
                     public ipc::MessageManagerCallback
{
protected:
  typedef mozilla::widget::PuppetWidget PuppetWidget;

public:
  TabChildBase();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TabChildBase)

  virtual bool WrapGlobalObject(JSContext* aCx,
                                JS::RealmOptions& aOptions,
                                JS::MutableHandle<JSObject*> aReflector) override
  {
    return mTabChildGlobal->WrapGlobalObject(aCx, aOptions, aReflector);
  }

  virtual nsIWebNavigation* WebNavigation() const = 0;
  virtual PuppetWidget* WebWidget() = 0;
  nsIPrincipal* GetPrincipal() { return mPrincipal; }
  virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                       const mozilla::layers::FrameMetrics::ViewID& aViewId,
                                       const Maybe<mozilla::layers::ZoomConstraints>& aConstraints) = 0;

  virtual ScreenIntSize GetInnerSize() = 0;

  // Get the Document for the top-level window in this tab.
  already_AddRefed<nsIDocument> GetDocument() const;

  // Get the pres-shell of the document for the top-level window in this tab.
  already_AddRefed<nsIPresShell> GetPresShell() const;

protected:
  virtual ~TabChildBase();

  // Wraps up a JSON object as a structured clone and sends it to the browser
  // chrome script.
  //
  // XXX/bug 780335: Do the work the browser chrome script does in C++ instead
  // so we don't need things like this.
  void DispatchMessageManagerMessage(const nsAString& aMessageName,
                                     const nsAString& aJSONData);

  void ProcessUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

  bool UpdateFrameHandler(const mozilla::layers::FrameMetrics& aFrameMetrics);

protected:
  RefPtr<TabChildGlobal> mTabChildGlobal;
  nsCOMPtr<nsIWebBrowserChrome3> mWebBrowserChrome;
};

class TabChild final : public TabChildBase,
                       public PBrowserChild,
                       public nsIWebBrowserChrome2,
                       public nsIEmbeddingSiteWindow,
                       public nsIWebBrowserChromeFocus,
                       public nsIInterfaceRequestor,
                       public nsIWindowProvider,
                       public nsSupportsWeakReference,
                       public nsITabChild,
                       public nsIObserver,
                       public TabContext,
                       public nsITooltipListener,
                       public mozilla::ipc::IShmemAllocator
{
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  typedef mozilla::dom::CoalescedMouseData CoalescedMouseData;
  typedef mozilla::dom::CoalescedWheelData CoalescedWheelData;
  typedef mozilla::layout::RenderFrameChild RenderFrameChild;
  typedef mozilla::layers::APZEventState APZEventState;
  typedef mozilla::layers::SetAllowedTouchBehaviorCallback SetAllowedTouchBehaviorCallback;
  typedef mozilla::layers::TouchBehaviorFlags TouchBehaviorFlags;

public:
  /**
   * Find TabChild of aTabId in the same content process of the
   * caller.
   */
  static already_AddRefed<TabChild> FindTabChild(const TabId& aTabId);

  // Return a list of all active TabChildren.
  static nsTArray<RefPtr<TabChild>> GetAll();

public:
  /**
   * Create a new TabChild object.
   */
  TabChild(nsIContentChild* aManager,
           const TabId& aTabId,
           TabGroup* aTabGroup,
           const TabContext& aContext,
           uint32_t aChromeFlags);

  nsresult Init();

  /** Return a TabChild with the given attributes. */
  static already_AddRefed<TabChild>
  Create(nsIContentChild* aManager, const TabId& aTabId,
         const TabId& aSameTabGroupAs,
         const TabContext& aContext, uint32_t aChromeFlags);

  // Let managees query if it is safe to send messages.
  bool IsDestroyed() const{ return mDestroyed; }

  const TabId GetTabId() const
  {
    MOZ_ASSERT(mUniqueId != 0);
    return mUniqueId;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWEBBROWSERCHROME2
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIWEBBROWSERCHROMEFOCUS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWINDOWPROVIDER
  NS_DECL_NSITABCHILD
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITOOLTIPLISTENER

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(TabChild, TabChildBase)

  FORWARD_SHMEM_ALLOCATOR_TO(PBrowserChild)

  TabChildGlobal* GetMessageManager()
  {
    return mTabChildGlobal;
  }

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     nsTArray<StructuredCloneData>* aRetVal,
                                     bool aIsSync) override;

  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  virtual bool
  DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                          const ViewID& aViewId,
                          const Maybe<ZoomConstraints>& aConstraints) override;

  virtual mozilla::ipc::IPCResult RecvLoadURL(const nsCString& aURI,
                                              const ShowInfo& aInfo) override;
  virtual mozilla::ipc::IPCResult
  RecvShow(const ScreenIntSize& aSize,
           const ShowInfo& aInfo,
           const bool& aParentIsActive,
           const nsSizeMode& aSizeMode) override;

  virtual mozilla::ipc::IPCResult
  RecvInitRendering(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                    const layers::LayersId& aLayersId,
                    const mozilla::layers::CompositorOptions& aCompositorOptions,
                    const bool& aLayersConnected,
                    PRenderFrameChild* aRenderFrame) override;

  virtual mozilla::ipc::IPCResult
  RecvUpdateDimensions(const mozilla::dom::DimensionInfo& aDimensionInfo) override;
  virtual mozilla::ipc::IPCResult
  RecvSizeModeChanged(const nsSizeMode& aSizeMode) override;

  mozilla::ipc::IPCResult RecvActivate();

  mozilla::ipc::IPCResult RecvDeactivate();

  MOZ_CAN_RUN_SCRIPT
  virtual mozilla::ipc::IPCResult RecvMouseEvent(const nsString& aType,
                                                 const float& aX,
                                                 const float& aY,
                                                 const int32_t& aButton,
                                                 const int32_t& aClickCount,
                                                 const int32_t& aModifiers,
                                                 const bool& aIgnoreRootScrollFrame) override;

  virtual mozilla::ipc::IPCResult RecvRealMouseMoveEvent(const mozilla::WidgetMouseEvent& aEvent,
                                                         const ScrollableLayerGuid& aGuid,
                                                         const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityRealMouseMoveEvent(const mozilla::WidgetMouseEvent& aEvent,
                                       const ScrollableLayerGuid& aGuid,
                                       const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult RecvSynthMouseMoveEvent(const mozilla::WidgetMouseEvent& aEvent,
                                                          const ScrollableLayerGuid& aGuid,
                                                          const uint64_t& aInputBlockId) override;
  virtual mozilla::ipc::IPCResult
  RecvNormalPrioritySynthMouseMoveEvent(const mozilla::WidgetMouseEvent& aEvent,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult RecvRealMouseButtonEvent(const mozilla::WidgetMouseEvent& aEvent,
                                                           const ScrollableLayerGuid& aGuid,
                                                           const uint64_t& aInputBlockId) override;
  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityRealMouseButtonEvent(const mozilla::WidgetMouseEvent& aEvent,
                                         const ScrollableLayerGuid& aGuid,
                                         const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult RecvRealDragEvent(const WidgetDragEvent& aEvent,
                                                    const uint32_t& aDragAction,
                                                    const uint32_t& aDropEffect,
                                                    const nsCString& aPrincipalURISpec) override;

  virtual mozilla::ipc::IPCResult
  RecvRealKeyEvent(const mozilla::WidgetKeyboardEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityRealKeyEvent(const mozilla::WidgetKeyboardEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult RecvMouseWheelEvent(const mozilla::WidgetWheelEvent& aEvent,
                                                      const ScrollableLayerGuid& aGuid,
                                                      const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityMouseWheelEvent(const mozilla::WidgetWheelEvent& aEvent,
                                    const ScrollableLayerGuid& aGuid,
                                    const uint64_t& aInputBlockId) override;

  virtual mozilla::ipc::IPCResult RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                                                     const ScrollableLayerGuid& aGuid,
                                                     const uint64_t& aInputBlockId,
                                                     const nsEventStatus& aApzResponse) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityRealTouchEvent(const WidgetTouchEvent& aEvent,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId,
                                   const nsEventStatus& aApzResponse) override;

  virtual mozilla::ipc::IPCResult
  RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                         const ScrollableLayerGuid& aGuid,
                         const uint64_t& aInputBlockId,
                         const nsEventStatus& aApzResponse) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                       const ScrollableLayerGuid& aGuid,
                                       const uint64_t& aInputBlockId,
                                       const nsEventStatus& aApzResponse) override;

  virtual mozilla::ipc::IPCResult RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                                              const nsCString& aResponse) override;

  virtual mozilla::ipc::IPCResult RecvPluginEvent(const WidgetPluginEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvCompositionEvent(const mozilla::WidgetCompositionEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPriorityCompositionEvent(
    const mozilla::WidgetCompositionEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvSelectionEvent(const mozilla::WidgetSelectionEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvNormalPrioritySelectionEvent(
    const mozilla::WidgetSelectionEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvPasteTransferable(const IPCDataTransfer& aDataTransfer,
                        const bool& aIsPrivateData,
                        const IPC::Principal& aRequestingPrincipal,
                        const uint32_t& aContentPolicyType) override;

  virtual mozilla::ipc::IPCResult
  RecvActivateFrameEvent(const nsString& aType, const bool& aCapture) override;

  virtual mozilla::ipc::IPCResult RecvLoadRemoteScript(const nsString& aURL,
                                                       const bool& aRunInGlobalScope) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMessage,
                                                   InfallibleTArray<CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;
  virtual mozilla::ipc::IPCResult
  RecvSwappedWithOtherRemoteLoader(const IPCTabContext& aContext) override;

  virtual PDocAccessibleChild*
  AllocPDocAccessibleChild(PDocAccessibleChild*, const uint64_t&,
                           const uint32_t&, const IAccessibleHolder&) override;

  virtual bool DeallocPDocAccessibleChild(PDocAccessibleChild*) override;

  virtual PColorPickerChild*
  AllocPColorPickerChild(const nsString& aTitle,
                         const nsString& aInitialColor) override;

  virtual bool DeallocPColorPickerChild(PColorPickerChild* aActor) override;

  virtual PFilePickerChild*
  AllocPFilePickerChild(const nsString& aTitle, const int16_t& aMode) override;

  virtual bool
  DeallocPFilePickerChild(PFilePickerChild* aActor) override;

  virtual PIndexedDBPermissionRequestChild*
  AllocPIndexedDBPermissionRequestChild(const Principal& aPrincipal) override;

  virtual bool
  DeallocPIndexedDBPermissionRequestChild(PIndexedDBPermissionRequestChild* aActor) override;

  virtual nsIWebNavigation* WebNavigation() const override
  {
    return mWebNav;
  }

  virtual PuppetWidget* WebWidget() override { return mPuppetWidget; }

  bool IsTransparent() const { return mIsTransparent; }

  void GetMaxTouchPoints(uint32_t* aTouchPoints)
  {
    *aTouchPoints = mMaxTouchPoints;
  }

  void SetMaxTouchPoints(uint32_t aMaxTouchPoints)
  {
    mMaxTouchPoints = aMaxTouchPoints;
  }

  ScreenOrientationInternal GetOrientation() const { return mOrientation; }

  void SetBackgroundColor(const nscolor& aColor);

  void NotifyPainted();

  void RequestEditCommands(nsIWidget::NativeKeyBindingsType aType,
                           const WidgetKeyboardEvent& aEvent,
                           nsTArray<CommandInt>& aCommands);

  /**
   * Signal to this TabChild that it should be made visible:
   * activated widget, retained layer tree, etc.  (Respectively,
   * made not visible.)
   */
  void MakeVisible();
  void MakeHidden();
  bool IsVisible();

  nsIContentChild* Manager() const { return mManager; }

  static inline TabChild*
  GetFrom(nsIDocShell* aDocShell)
  {
    if (!aDocShell) {
      return nullptr;
    }

    nsCOMPtr<nsITabChild> tc = aDocShell->GetTabChild();
    return static_cast<TabChild*>(tc.get());
  }

  static inline TabChild*
  GetFrom(mozIDOMWindow* aWindow)
  {
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
    return GetFrom(docShell);
  }

  static inline TabChild*
  GetFrom(mozIDOMWindowProxy* aWindow)
  {
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
    return GetFrom(docShell);
  }

  static TabChild* GetFrom(nsIPresShell* aPresShell);
  static TabChild* GetFrom(layers::LayersId aLayersId);

  layers::LayersId GetLayersId() { return mLayersId; }
  Maybe<bool> IsLayersConnected() { return mLayersConnected; }

  void DidComposite(mozilla::layers::TransactionId aTransactionId,
                    const TimeStamp& aCompositeStart,
                    const TimeStamp& aCompositeEnd);

  void DidRequestComposite(const TimeStamp& aCompositeReqStart,
                           const TimeStamp& aCompositeReqEnd);

  void ClearCachedResources();
  void InvalidateLayers();
  void ReinitRendering();
  void ReinitRenderingForDeviceReset();

  static inline TabChild* GetFrom(nsIDOMWindow* aWindow)
  {
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
    return GetFrom(docShell);
  }

  virtual mozilla::ipc::IPCResult RecvUIResolutionChanged(const float& aDpi,
                                                          const int32_t& aRounding,
                                                          const double& aScale) override;

  virtual mozilla::ipc::IPCResult
  RecvThemeChanged(nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache) override;

  virtual mozilla::ipc::IPCResult
  RecvHandleAccessKey(const WidgetKeyboardEvent& aEvent,
                      nsTArray<uint32_t>&& aCharCodes) override;

  virtual mozilla::ipc::IPCResult RecvSetUseGlobalHistory(const bool& aUse) override;

  virtual mozilla::ipc::IPCResult RecvHandledWindowedPluginKeyEvent(
    const mozilla::NativeEventData& aKeyEventData,
    const bool& aIsConsumed) override;

  virtual mozilla::ipc::IPCResult RecvPrint(const uint64_t& aOuterWindowID,
                                            const PrintData& aPrintData) override;

  virtual mozilla::ipc::IPCResult RecvUpdateNativeWindowHandle(const uintptr_t& aNewHandle) override;

  /**
   * Native widget remoting protocol for use with windowed plugins with e10s.
   */
  PPluginWidgetChild* AllocPPluginWidgetChild() override;

  bool DeallocPPluginWidgetChild(PPluginWidgetChild* aActor) override;

#ifdef XP_WIN
  nsresult CreatePluginWidget(nsIWidget* aParent, nsIWidget** aOut);
#endif

  virtual PPaymentRequestChild*
  AllocPPaymentRequestChild() override;

  virtual bool
  DeallocPPaymentRequestChild(PPaymentRequestChild* aActor) override;

  LayoutDeviceIntPoint GetClientOffset() const { return mClientOffset; }
  LayoutDeviceIntPoint GetChromeOffset() const { return mChromeOffset; };

  bool IPCOpen() const { return mIPCOpen; }

  bool ParentIsActive() const
  {
    return mParentIsActive;
  }

  const mozilla::layers::CompositorOptions& GetCompositorOptions() const;
  bool AsyncPanZoomEnabled() const;

  virtual ScreenIntSize GetInnerSize() override;

  // Call RecvShow(nsIntSize(0, 0)) and block future calls to RecvShow().
  void DoFakeShow(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                  const layers::LayersId& aLayersId,
                  const mozilla::layers::CompositorOptions& aCompositorOptions,
                  PRenderFrameChild* aRenderFrame,
                  const ShowInfo& aShowInfo);

  void ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                 uint64_t aInputBlockId,
                                 bool aPreventDefault) const;
  void SetTargetAPZC(uint64_t aInputBlockId,
                    const nsTArray<ScrollableLayerGuid>& aTargets) const;
  MOZ_CAN_RUN_SCRIPT
  mozilla::ipc::IPCResult RecvHandleTap(const layers::GeckoContentController::TapType& aType,
                                        const LayoutDevicePoint& aPoint,
                                        const Modifiers& aModifiers,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId) override;

  MOZ_CAN_RUN_SCRIPT
  mozilla::ipc::IPCResult
  RecvNormalPriorityHandleTap(const layers::GeckoContentController::TapType& aType,
                              const LayoutDevicePoint& aPoint,
                              const Modifiers& aModifiers,
                              const ScrollableLayerGuid& aGuid,
                              const uint64_t& aInputBlockId) override;

  void SetAllowedTouchBehavior(uint64_t aInputBlockId,
                               const nsTArray<TouchBehaviorFlags>& aFlags) const;

  bool UpdateFrame(const FrameMetrics& aFrameMetrics);
  bool NotifyAPZStateChange(const ViewID& aViewId,
                            const layers::GeckoContentController::APZStateChange& aChange,
                            const int& aArg);
  void StartScrollbarDrag(const layers::AsyncDragMetrics& aDragMetrics);
  void ZoomToRect(const uint32_t& aPresShellId,
                  const FrameMetrics::ViewID& aViewId,
                  const CSSRect& aRect,
                  const uint32_t& aFlags);

  // Request that the docshell be marked as active.
  void PaintWhileInterruptingJS(uint64_t aLayerObserverEpoch,
                                bool aForceRepaint);

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  uintptr_t GetNativeWindowHandle() const { return mNativeWindowHandle; }
#endif

  // These methods return `true` if this TabChild is currently awaiting a
  // Large-Allocation header.
  bool StopAwaitingLargeAlloc();
  bool IsAwaitingLargeAlloc();

  mozilla::dom::TabGroup* TabGroup();

#if defined(ACCESSIBILITY)
  void SetTopLevelDocAccessibleChild(PDocAccessibleChild* aTopLevelChild)
  {
    mTopLevelDocAccessibleChild = aTopLevelChild;
  }

  PDocAccessibleChild* GetTopLevelDocAccessibleChild()
  {
    return mTopLevelDocAccessibleChild;
  }
#endif

  void AddPendingDocShellBlocker();
  void RemovePendingDocShellBlocker();

  // The HANDLE object for the widget this TabChild in.
  WindowsHandle WidgetNativeData()
  {
    return mWidgetNativeData;
  }

  // Prepare to dispatch all coalesced mousemove events. We'll move all data
  // in mCoalescedMouseData to a nsDeque; then we start processing them. We
  // can't fetch the coalesced event one by one and dispatch it because we may
  // reentry the event loop and access to the same hashtable. It's called when
  // dispatching some mouse events other than mousemove.
  void FlushAllCoalescedMouseData();
  void ProcessPendingCoalescedMouseDataAndDispatchEvents();

  void HandleRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId);

  static bool HasVisibleTabs()
  {
    return sVisibleTabs && !sVisibleTabs->IsEmpty();
  }

  // Returns the set of TabChilds that are currently rendering layers. There
  // can be multiple TabChilds in this state if Firefox has multiple windows
  // open or is warming tabs up. There can also be zero TabChilds in this
  // state. Note that this function should only be called if HasVisibleTabs()
  // returns true.
  static const nsTHashtable<nsPtrHashKey<TabChild>>& GetVisibleTabs()
  {
    MOZ_ASSERT(HasVisibleTabs());
    return *sVisibleTabs;
  }

protected:
  virtual ~TabChild();

  virtual PRenderFrameChild* AllocPRenderFrameChild() override;

  virtual bool DeallocPRenderFrameChild(PRenderFrameChild* aFrame) override;

  virtual mozilla::ipc::IPCResult RecvDestroy() override;

  virtual mozilla::ipc::IPCResult RecvSetDocShellIsActive(const bool& aIsActive) override;

  virtual mozilla::ipc::IPCResult RecvRenderLayers(const bool& aEnabled, const bool& aForce, const uint64_t& aLayerObserverEpoch) override;

  virtual mozilla::ipc::IPCResult RecvNavigateByKey(const bool& aForward,
                                                    const bool& aForDocumentNavigation) override;

  virtual mozilla::ipc::IPCResult RecvRequestNotifyAfterRemotePaint() override;

  virtual mozilla::ipc::IPCResult RecvSuppressDisplayport(const bool& aEnabled) override;

  virtual mozilla::ipc::IPCResult RecvParentActivated(const bool& aActivated) override;

  virtual mozilla::ipc::IPCResult RecvSetKeyboardIndicators(const UIStateChangeType& aShowAccelerators,
                                                            const UIStateChangeType& aShowFocusRings) override;

  virtual mozilla::ipc::IPCResult RecvStopIMEStateManagement() override;

  virtual mozilla::ipc::IPCResult RecvAwaitLargeAlloc() override;

  virtual mozilla::ipc::IPCResult RecvSetWindowName(const nsString& aName) override;

  virtual mozilla::ipc::IPCResult RecvSetOriginAttributes(const OriginAttributes& aOriginAttributes) override;

  virtual mozilla::ipc::IPCResult RecvSetWidgetNativeData(const WindowsHandle& aWidgetNativeData) override;

private:
  void HandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers,
                       const ScrollableLayerGuid& aGuid);

  // Notify others that our TabContext has been updated.
  //
  // You should call this after calling TabContext::SetTabContext().  We also
  // call this during Init().
  //
  // @param aIsPreallocated  true if this is called for Preallocated Tab.
  void NotifyTabContextUpdated(bool aIsPreallocated);

  // Update the frameType on our docshell.
  void UpdateFrameType();

  void ActorDestroy(ActorDestroyReason why) override;

  bool InitTabChildGlobal();

  void InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                          const layers::LayersId& aLayersId,
                          const mozilla::layers::CompositorOptions& aCompositorOptions,
                          PRenderFrameChild* aRenderFrame);
  void InitAPZState();

  void DestroyWindow();

  void ApplyShowInfo(const ShowInfo& aInfo);

  bool HasValidInnerSize();

  void SetTabId(const TabId& aTabId);

  ScreenIntRect GetOuterRect();

  void SetUnscaledInnerSize(const CSSSize& aSize)
  {
    mUnscaledInnerSize = aSize;
  }

  bool SkipRepeatedKeyEvent(const WidgetKeyboardEvent& aEvent);

  void UpdateRepeatedKeyEventEndTime(const WidgetKeyboardEvent& aEvent);

  bool MaybeCoalesceWheelEvent(const WidgetWheelEvent& aEvent,
                               const ScrollableLayerGuid& aGuid,
                               const uint64_t& aInputBlockId,
                               bool* aIsNextWheelEvent);

  void MaybeDispatchCoalescedWheelEvent();

  /**
   * Dispatch aEvent on aEvent.mWidget.
   */
  nsEventStatus DispatchWidgetEventViaAPZ(WidgetGUIEvent& aEvent);

  void DispatchWheelEvent(const WidgetWheelEvent& aEvent,
                          const ScrollableLayerGuid& aGuid,
                          const uint64_t& aInputBlockId);

  void InternalSetDocShellIsActive(bool aIsActive);

  bool CreateRemoteLayerManager(mozilla::layers::PCompositorBridgeChild* aCompositorChild);

  class DelayedDeleteRunnable;

  TextureFactoryIdentifier mTextureFactoryIdentifier;
  nsCOMPtr<nsIWebNavigation> mWebNav;
  RefPtr<mozilla::dom::TabGroup> mTabGroup;
  RefPtr<PuppetWidget> mPuppetWidget;
  nsCOMPtr<nsIURI> mLastURI;
  RenderFrameChild* mRemoteFrame;
  RefPtr<nsIContentChild> mManager;
  uint32_t mChromeFlags;
  uint32_t mMaxTouchPoints;
  layers::LayersId mLayersId;
  int64_t mBeforeUnloadListeners;
  CSSRect mUnscaledOuterRect;
  Maybe<bool> mLayersConnected;
  bool mDidFakeShow;
  bool mNotified;
  bool mTriedBrowserInit;
  ScreenOrientationInternal mOrientation;

  bool mIgnoreKeyPressEvent;
  RefPtr<APZEventState> mAPZEventState;
  SetAllowedTouchBehaviorCallback mSetAllowedTouchBehaviorCallback;
  bool mHasValidInnerSize;
  bool mDestroyed;
  // Position of client area relative to the outer window
  LayoutDeviceIntPoint mClientOffset;
  // Position of tab, relative to parent widget (typically the window)
  LayoutDeviceIntPoint mChromeOffset;
  TabId mUniqueId;

  // Holds the compositor options for the compositor rendering this tab,
  // once we find out which compositor that is.
  Maybe<mozilla::layers::CompositorOptions> mCompositorOptions;

  friend class ContentChild;

  bool mIsTransparent;

  bool mIPCOpen;
  bool mParentIsActive;
  CSSSize mUnscaledInnerSize;
  bool mDidSetRealShowInfo;
  bool mDidLoadURLInit;
  bool mAwaitingLA;

  bool mSkipKeyPress;

  // Store the end time of the handling of the last repeated keydown/keypress
  // event so that in case event handling takes time, some repeated events can
  // be skipped to not flood child process.
  mozilla::TimeStamp mRepeatedKeyEventTime;

  // Similar to mRepeatedKeyEventTime, store the end time (from parent process)
  // of handling the last repeated wheel event so that in case event handling
  // takes time, some repeated events can be skipped to not flood child process.
  mozilla::TimeStamp mLastWheelProcessedTimeFromParent;
  mozilla::TimeDuration mLastWheelProcessingDuration;

  // Hash table to track coalesced mousemove events for different pointers.
  nsClassHashtable<nsUint32HashKey, CoalescedMouseData> mCoalescedMouseData;

  nsDeque mToBeDispatchedMouseData;

  CoalescedWheelData mCoalescedWheelData;
  RefPtr<CoalescedMouseMoveFlusher> mCoalescedMouseEventFlusher;

  RefPtr<layers::IAPZCTreeManager> mApzcTreeManager;

  // The most recently seen layer observer epoch in RecvSetDocShellIsActive.
  uint64_t mLayerObserverEpoch;

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  // The handle associated with the native window that contains this tab
  uintptr_t mNativeWindowHandle;
#endif // defined(XP_WIN)

#if defined(ACCESSIBILITY)
  PDocAccessibleChild* mTopLevelDocAccessibleChild;
#endif
  bool mCoalesceMouseMoveEvents;

  // In some circumstances, a DocShell might be in a state where it is
  // "blocked", and we should not attempt to change its active state or
  // the underlying PresShell state until the DocShell becomes unblocked.
  // It is possible, however, for the parent process to send commands to
  // change those states while the DocShell is blocked. We store those
  // states temporarily as "pending", and only apply them once the DocShell
  // is no longer blocked.
  bool mPendingDocShellIsActive;
  bool mPendingDocShellReceivedMessage;
  bool mPendingRenderLayers;
  bool mPendingRenderLayersReceivedMessage;
  uint64_t mPendingLayerObserverEpoch;
  // When mPendingDocShellBlockers is greater than 0, the DocShell is blocked,
  // and once it reaches 0, it is no longer blocked.
  uint32_t mPendingDocShellBlockers;

  WindowsHandle mWidgetNativeData;

  // This state is used to keep track of the current visible tabs (the ones rendering
  // layers). There may be more than one if there are multiple browser windows open, or
  // tabs are being warmed up. There may be none if this process does not host any
  // visible or warming tabs.
  static nsTHashtable<nsPtrHashKey<TabChild>>* sVisibleTabs;

  DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TabChild_h

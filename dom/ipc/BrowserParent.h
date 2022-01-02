/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowserParent_h
#define mozilla_dom_BrowserParent_h

#include <utility>

#include "LiveResizeListener.h"
#include "Units.h"
#include "js/TypeDecls.h"
#include "mozilla/ContentCache.h"
#include "mozilla/EventForwards.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/VsyncParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/layout/RemoteLayerTreeOwner.h"
#include "nsCOMPtr.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMEventListener.h"
#include "nsIRemoteTab.h"
#include "nsIWidget.h"
#include "nsWeakReference.h"

class imgIContainer;
class nsCycleCollectionTraversalCallback;
class nsDocShellLoadState;
class nsFrameLoader;
class nsIBrowser;
class nsIContent;
class nsIContentSecurityPolicy;
class nsIDocShell;
class nsILoadContext;
class nsIPrincipal;
class nsIRequest;
class nsIURI;
class nsIWebBrowserPersistDocumentReceiver;
class nsIWebProgress;
class nsIXULBrowserWindow;
class nsPIDOMWindowOuter;

namespace mozilla {

namespace a11y {
class DocAccessibleParent;
}

namespace widget {
struct IMENotification;
}  // namespace widget

namespace gfx {
class SourceSurface;
}  // namespace gfx

namespace dom {

class CanonicalBrowsingContext;
class ClonedMessageData;
class ContentParent;
class Element;
class DataTransfer;
class BrowserHost;
class BrowserBridgeParent;

namespace ipc {
class StructuredCloneData;
}  // namespace ipc

/**
 * BrowserParent implements the parent actor part of the PBrowser protocol. See
 * PBrowser for more information.
 */
class BrowserParent final : public PBrowserParent,
                            public nsIDOMEventListener,
                            public nsIAuthPromptProvider,
                            public nsSupportsWeakReference,
                            public TabContext,
                            public LiveResizeListener {
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;
  using TapType = GeckoContentController_TapType;

  friend class PBrowserParent;

  virtual ~BrowserParent();

 public:
  // Helper class for ContentParent::RecvCreateWindow.
  struct AutoUseNewTab;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIAUTHPROMPTPROVIDER
  // nsIDOMEventListener interfaces
  NS_DECL_NSIDOMEVENTLISTENER

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(BrowserParent, nsIDOMEventListener)

  BrowserParent(ContentParent* aManager, const TabId& aTabId,
                const TabContext& aContext,
                CanonicalBrowsingContext* aBrowsingContext,
                uint32_t aChromeFlags);

  /**
   * Returns the focused BrowserParent or nullptr if chrome or another app
   * is focused.
   */
  static BrowserParent* GetFocused();

  static BrowserParent* GetLastMouseRemoteTarget();

  static BrowserParent* GetFrom(nsFrameLoader* aFrameLoader);

  static BrowserParent* GetFrom(PBrowserParent* aBrowserParent);

  static BrowserParent* GetFrom(nsIContent* aContent);

  static BrowserParent* GetBrowserParentFromLayersId(
      layers::LayersId aLayersId);

  static TabId GetTabIdFrom(nsIDocShell* docshell);

  const TabId GetTabId() const { return mTabId; }

  ContentParent* Manager() const { return mManager; }

  CanonicalBrowsingContext* GetBrowsingContext() { return mBrowsingContext; }

  already_AddRefed<nsILoadContext> GetLoadContext();

  Element* GetOwnerElement() const { return mFrameElement; }

  nsIBrowserDOMWindow* GetBrowserDOMWindow() const { return mBrowserDOMWindow; }

  already_AddRefed<nsPIDOMWindowOuter> GetParentWindowOuter();

  already_AddRefed<nsIWidget> GetTopLevelWidget();

  // Returns the closest widget for our frameloader's content.
  already_AddRefed<nsIWidget> GetWidget() const;

  // Returns the top-level widget for our frameloader's document.
  already_AddRefed<nsIWidget> GetDocWidget() const;

  /**
   * Returns the widget which may have native focus and handles text input
   * like keyboard input, IME, etc.
   */
  already_AddRefed<nsIWidget> GetTextInputHandlingWidget() const;

  nsIXULBrowserWindow* GetXULBrowserWindow();

  static uint32_t GetMaxTouchPoints(Element* aElement);
  uint32_t GetMaxTouchPoints() { return GetMaxTouchPoints(mFrameElement); }

  /**
   * Return the top level DocAccessibleParent for this BrowserParent.
   * Note that in the case of an out-of-process iframe, the returned actor
   * might not be at the top level of the DocAccessibleParent tree; i.e. it
   * might have a parent. However, it will be at the top level in its content
   * process. That is, doc->IsTopLevelInContentProcess() will always be true,
   * but doc->IsTopLevel() might not.
   */
  a11y::DocAccessibleParent* GetTopLevelDocAccessible() const;

  LayersId GetLayersId() const;

  // Returns the BrowserBridgeParent if this BrowserParent is for an
  // out-of-process iframe and nullptr otherwise.
  BrowserBridgeParent* GetBrowserBridgeParent() const;

  // Returns the BrowserHost if this BrowserParent is for a top-level browser
  // and nullptr otherwise.
  BrowserHost* GetBrowserHost() const;

  ParentShowInfo GetShowInfo();

  // Get the content principal from the owner element.
  already_AddRefed<nsIPrincipal> GetContentPrincipal() const;

  /**
   * Let managees query if Destroy() is already called so they don't send out
   * messages when the PBrowser actor is being destroyed.
   */
  bool IsDestroyed() const { return mIsDestroyed; }

  /**
   * Returns whether we're in the process of creating a new window (from
   * window.open). If so, LoadURL calls are being skipped until everything is
   * set up. For further details, see `mCreatingWindow` below.
   */
  bool CreatingWindow() const { return mCreatingWindow; }

  /*
   * Visit each BrowserParent in the tree formed by PBrowser and
   * PBrowserBridge, including `this`.
   */
  template <typename Callback>
  void VisitAll(Callback aCallback) {
    aCallback(this);
    VisitAllDescendants(aCallback);
  }

  /*
   * Visit each BrowserParent in the tree formed by PBrowser and
   * PBrowserBridge, excluding `this`.
   */
  template <typename Callback>
  void VisitAllDescendants(Callback aCallback) {
    const auto& browserBridges = ManagedPBrowserBridgeParent();
    for (const auto& key : browserBridges) {
      BrowserBridgeParent* browserBridge =
          static_cast<BrowserBridgeParent*>(key);
      BrowserParent* browserParent = browserBridge->GetBrowserParent();

      aCallback(browserParent);
      browserParent->VisitAllDescendants(aCallback);
    }
  }

  /*
   * Visit each BrowserBridgeParent that is a child of this BrowserParent.
   */
  template <typename Callback>
  void VisitChildren(Callback aCallback) {
    const auto& browserBridges = ManagedPBrowserBridgeParent();
    for (const auto& key : browserBridges) {
      BrowserBridgeParent* browserBridge =
          static_cast<BrowserBridgeParent*>(key);
      aCallback(browserBridge);
    }
  }

  void SetOwnerElement(Element* aElement);

  void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow) {
    mBrowserDOMWindow = aBrowserDOMWindow;
  }

  void SwapFrameScriptsFrom(nsTArray<FrameScriptInfo>& aFrameScripts) {
    aFrameScripts.SwapElements(mDelayedFrameScripts);
  }

  void CacheFrameLoader(nsFrameLoader* aFrameLoader);

  void Destroy();

  void RemoveWindowListeners();

  void AddWindowListeners();

  mozilla::ipc::IPCResult RecvDidUnsuppressPainting();
  mozilla::ipc::IPCResult RecvDidUnsuppressPaintingNormalPriority() {
    return RecvDidUnsuppressPainting();
  }
  mozilla::ipc::IPCResult RecvMoveFocus(const bool& aForward,
                                        const bool& aForDocumentNavigation);

  mozilla::ipc::IPCResult RecvSizeShellTo(const uint32_t& aFlags,
                                          const int32_t& aWidth,
                                          const int32_t& aHeight,
                                          const int32_t& aShellItemWidth,
                                          const int32_t& aShellItemHeight);

  mozilla::ipc::IPCResult RecvDropLinks(nsTArray<nsString>&& aLinks);

  mozilla::ipc::IPCResult RecvEvent(const RemoteDOMEvent& aEvent);

  mozilla::ipc::IPCResult RecvReplyKeyEvent(const WidgetKeyboardEvent& aEvent);

  mozilla::ipc::IPCResult RecvAccessKeyNotHandled(
      const WidgetKeyboardEvent& aEvent);

  mozilla::ipc::IPCResult RecvRegisterProtocolHandler(const nsString& aScheme,
                                                      nsIURI* aHandlerURI,
                                                      const nsString& aTitle,
                                                      nsIURI* aDocURI);

  mozilla::ipc::IPCResult RecvOnStateChange(
      const WebProgressData& aWebProgressData, const RequestData& aRequestData,
      const uint32_t aStateFlags, const nsresult aStatus,
      const Maybe<WebProgressStateChangeData>& aStateChangeData);

  mozilla::ipc::IPCResult RecvOnProgressChange(const int32_t aCurTotalProgres,
                                               const int32_t aMaxTotalProgress);

  mozilla::ipc::IPCResult RecvOnLocationChange(
      const WebProgressData& aWebProgressData, const RequestData& aRequestData,
      nsIURI* aLocation, const uint32_t aFlags, const bool aCanGoBack,
      const bool aCanGoForward,
      const Maybe<WebProgressLocationChangeData>& aLocationChangeData);

  mozilla::ipc::IPCResult RecvOnStatusChange(const nsString& aMessage);

  mozilla::ipc::IPCResult RecvNotifyContentBlockingEvent(
      const uint32_t& aEvent, const RequestData& aRequestData,
      const bool aBlocked, const nsACString& aTrackingOrigin,
      nsTArray<nsCString>&& aTrackingFullHashes,
      const Maybe<mozilla::ContentBlockingNotifier::
                      StorageAccessPermissionGrantedReason>& aReason);

  mozilla::ipc::IPCResult RecvSetAllowDeprecatedTls(bool value);

  mozilla::ipc::IPCResult RecvNavigationFinished();

  already_AddRefed<nsIBrowser> GetBrowser();

  already_AddRefed<CanonicalBrowsingContext> BrowsingContextForWebProgress(
      const WebProgressData& aWebProgressData);

  mozilla::ipc::IPCResult RecvSessionStoreUpdate(
      const Maybe<nsCString>& aDocShellCaps, const Maybe<bool>& aPrivatedMode,
      const bool aNeedCollectSHistory, const uint32_t& aEpoch);

  mozilla::ipc::IPCResult RecvIntrinsicSizeOrRatioChanged(
      const Maybe<IntrinsicSize>& aIntrinsicSize,
      const Maybe<AspectRatio>& aIntrinsicRatio);

  mozilla::ipc::IPCResult RecvSyncMessage(
      const nsString& aMessage, const ClonedMessageData& aData,
      nsTArray<ipc::StructuredCloneData>* aRetVal);

  mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMessage,
                                           const ClonedMessageData& aData);

  mozilla::ipc::IPCResult RecvNotifyIMEFocus(
      const ContentCache& aContentCache,
      const widget::IMENotification& aEventMessage,
      NotifyIMEFocusResolver&& aResolve);

  mozilla::ipc::IPCResult RecvNotifyIMETextChange(
      const ContentCache& aContentCache,
      const widget::IMENotification& aEventMessage);

  mozilla::ipc::IPCResult RecvNotifyIMECompositionUpdate(
      const ContentCache& aContentCache,
      const widget::IMENotification& aEventMessage);

  mozilla::ipc::IPCResult RecvNotifyIMESelection(
      const ContentCache& aContentCache,
      const widget::IMENotification& aEventMessage);

  mozilla::ipc::IPCResult RecvUpdateContentCache(
      const ContentCache& aContentCache);

  mozilla::ipc::IPCResult RecvNotifyIMEMouseButtonEvent(
      const widget::IMENotification& aEventMessage, bool* aConsumedByIME);

  mozilla::ipc::IPCResult RecvNotifyIMEPositionChange(
      const ContentCache& aContentCache,
      const widget::IMENotification& aEventMessage);

  mozilla::ipc::IPCResult RecvOnEventNeedingAckHandled(
      const EventMessage& aMessage);

  mozilla::ipc::IPCResult RecvRequestIMEToCommitComposition(
      const bool& aCancel, bool* aIsCommitted, nsString* aCommittedString);

  mozilla::ipc::IPCResult RecvGetInputContext(widget::IMEState* aIMEState);

  mozilla::ipc::IPCResult RecvSetInputContext(
      const widget::InputContext& aContext,
      const widget::InputContextAction& aAction);

  mozilla::ipc::IPCResult RecvRequestFocus(const bool& aCanRaise,
                                           const CallerType aCallerType);

  mozilla::ipc::IPCResult RecvWheelZoomChange(bool aIncrease);

  mozilla::ipc::IPCResult RecvLookUpDictionary(
      const nsString& aText, nsTArray<mozilla::FontRange>&& aFontRangeArray,
      const bool& aIsVertical, const LayoutDeviceIntPoint& aPoint);

  mozilla::ipc::IPCResult RecvEnableDisableCommands(
      const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aAction,
      nsTArray<nsCString>&& aEnabledCommands,
      nsTArray<nsCString>&& aDisabledCommands);

  mozilla::ipc::IPCResult RecvSetCursor(
      const nsCursor& aValue, const bool& aHasCustomCursor,
      const nsCString& aCursorData, const uint32_t& aWidth,
      const uint32_t& aHeight, const float& aResolutionX,
      const float& aResolutionY, const uint32_t& aStride,
      const gfx::SurfaceFormat& aFormat, const uint32_t& aHotspotX,
      const uint32_t& aHotspotY, const bool& aForce);

  mozilla::ipc::IPCResult RecvSetLinkStatus(const nsString& aStatus);

  mozilla::ipc::IPCResult RecvShowTooltip(const uint32_t& aX,
                                          const uint32_t& aY,
                                          const nsString& aTooltip,
                                          const nsString& aDirection);

  mozilla::ipc::IPCResult RecvHideTooltip();

  mozilla::ipc::IPCResult RecvDispatchFocusToTopLevelWindow();

  mozilla::ipc::IPCResult RecvRespondStartSwipeEvent(
      const uint64_t& aInputBlockId, const bool& aStartSwipe);

  mozilla::ipc::IPCResult RecvDispatchWheelEvent(
      const mozilla::WidgetWheelEvent& aEvent);

  mozilla::ipc::IPCResult RecvDispatchMouseEvent(
      const mozilla::WidgetMouseEvent& aEvent);

  mozilla::ipc::IPCResult RecvDispatchKeyboardEvent(
      const mozilla::WidgetKeyboardEvent& aEvent);

  mozilla::ipc::IPCResult RecvDispatchTouchEvent(
      const mozilla::WidgetTouchEvent& aEvent);

  mozilla::ipc::IPCResult RecvScrollRectIntoView(
      const nsRect& aRect, const ScrollAxis& aVertical,
      const ScrollAxis& aHorizontal, const ScrollFlags& aScrollFlags,
      const int32_t& aAppUnitsPerDevPixel);

  PColorPickerParent* AllocPColorPickerParent(const nsString& aTitle,
                                              const nsString& aInitialColor);

  bool DeallocPColorPickerParent(PColorPickerParent* aColorPicker);

  PVsyncParent* AllocPVsyncParent();

  bool DeallocPVsyncParent(PVsyncParent* aActor);

#ifdef ACCESSIBILITY
  PDocAccessibleParent* AllocPDocAccessibleParent(PDocAccessibleParent*,
                                                  const uint64_t&,
                                                  const uint32_t&,
                                                  const IAccessibleHolder&);
  bool DeallocPDocAccessibleParent(PDocAccessibleParent*);
  virtual mozilla::ipc::IPCResult RecvPDocAccessibleConstructor(
      PDocAccessibleParent* aDoc, PDocAccessibleParent* aParentDoc,
      const uint64_t& aParentID, const uint32_t& aMsaaID,
      const IAccessibleHolder& aDocCOMProxy) override;
#endif

  mozilla::ipc::IPCResult RecvNewWindowGlobal(
      ManagedEndpoint<PWindowGlobalParent>&& aEndpoint,
      const WindowGlobalInit& aInit);

  mozilla::ipc::IPCResult RecvIsWindowSupportingProtectedMedia(
      const uint64_t& aOuterWindowID,
      IsWindowSupportingProtectedMediaResolver&& aResolve);

  mozilla::ipc::IPCResult RecvIsWindowSupportingWebVR(
      const uint64_t& aOuterWindowID,
      IsWindowSupportingWebVRResolver&& aResolve);

  void LoadURL(nsDocShellLoadState* aLoadState);

  void ResumeLoad(uint64_t aPendingSwitchID);

  void InitRendering();
  bool AttachWindowRenderer();
  void MaybeShowFrame();

  bool Show(const OwnerShowInfo&);

  void UpdateDimensions(const nsIntRect& aRect, const ScreenIntSize& aSize);

  DimensionInfo GetDimensionInfo();

  nsresult UpdatePosition();

  // Notify position update to all descendant documents in this browser parent.
  // NOTE: This should use only for browsers in popup windows attached to the
  // main browser window.
  void NotifyPositionUpdatedForContentsInPopup();

  void SizeModeChanged(const nsSizeMode& aSizeMode);

  void HandleAccessKey(const WidgetKeyboardEvent& aEvent,
                       nsTArray<uint32_t>& aCharCodes);

#if defined(MOZ_WIDGET_ANDROID)
  void DynamicToolbarMaxHeightChanged(ScreenIntCoord aHeight);
  void DynamicToolbarOffsetChanged(ScreenIntCoord aOffset);
#endif

  void Activate(uint64_t aActionId);

  void Deactivate(bool aWindowLowering, uint64_t aActionId);

  void MouseEnterIntoWidget();

  bool MapEventCoordinatesForChildProcess(mozilla::WidgetEvent* aEvent);

  void MapEventCoordinatesForChildProcess(const LayoutDeviceIntPoint& aOffset,
                                          mozilla::WidgetEvent* aEvent);

  LayoutDeviceToCSSScale GetLayoutDeviceToCSSScale();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY mozilla::ipc::IPCResult
  RecvRequestNativeKeyBindings(const uint32_t& aType,
                               const mozilla::WidgetKeyboardEvent& aEvent,
                               nsTArray<mozilla::CommandInt>* aCommands);

  mozilla::ipc::IPCResult RecvSynthesizeNativeKeyEvent(
      const int32_t& aNativeKeyboardLayout, const int32_t& aNativeKeyCode,
      const uint32_t& aModifierFlags, const nsString& aCharacters,
      const nsString& aUnmodifiedCharacters, const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeMouseEvent(
      const LayoutDeviceIntPoint& aPoint, const uint32_t& aNativeMessage,
      const int16_t& aButton, const uint32_t& aModifierFlags,
      const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeMouseMove(
      const LayoutDeviceIntPoint& aPoint, const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeMouseScrollEvent(
      const LayoutDeviceIntPoint& aPoint, const uint32_t& aNativeMessage,
      const double& aDeltaX, const double& aDeltaY, const double& aDeltaZ,
      const uint32_t& aModifierFlags, const uint32_t& aAdditionalFlags,
      const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeTouchPoint(
      const uint32_t& aPointerId, const TouchPointerState& aPointerState,
      const LayoutDeviceIntPoint& aPoint, const double& aPointerPressure,
      const uint32_t& aPointerOrientation, const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeTouchPadPinch(
      const TouchpadGesturePhase& aEventPhase, const float& aScale,
      const LayoutDeviceIntPoint& aPoint, const int32_t& aModifierFlags);

  mozilla::ipc::IPCResult RecvSynthesizeNativeTouchTap(
      const LayoutDeviceIntPoint& aPoint, const bool& aLongTap,
      const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvClearNativeTouchSequence(
      const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativePenInput(
      const uint32_t& aPointerId, const TouchPointerState& aPointerState,
      const LayoutDeviceIntPoint& aPoint, const double& aPressure,
      const uint32_t& aRotation, const int32_t& aTiltX, const int32_t& aTiltY,
      const int32_t& aButton, const uint64_t& aObserverId);

  mozilla::ipc::IPCResult RecvSynthesizeNativeTouchpadDoubleTap(
      const LayoutDeviceIntPoint& aPoint, const uint32_t& aModifierFlags);

  mozilla::ipc::IPCResult RecvSynthesizeNativeTouchpadPan(
      const TouchpadGesturePhase& aEventPhase,
      const LayoutDeviceIntPoint& aPoint, const double& aDeltaX,
      const double& aDeltaY, const int32_t& aModifierFlags);

  mozilla::ipc::IPCResult RecvLockNativePointer();

  mozilla::ipc::IPCResult RecvUnlockNativePointer();

  /**
   * The following Send*Event() marks aEvent as posted to remote process if
   * it succeeded.  So, you can check the result with
   * aEvent.HasBeenPostedToRemoteProcess().
   */
  void SendRealMouseEvent(WidgetMouseEvent& aEvent);

  void SendRealDragEvent(WidgetDragEvent& aEvent, uint32_t aDragAction,
                         uint32_t aDropEffect, nsIPrincipal* aPrincipal,
                         nsIContentSecurityPolicy* aCsp);

  void SendMouseWheelEvent(WidgetWheelEvent& aEvent);

  /**
   * Only when the event is synthesized, retrieving writing mode may flush
   * the layout.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SendRealKeyEvent(
      WidgetKeyboardEvent& aEvent);

  void SendRealTouchEvent(WidgetTouchEvent& aEvent);

  /**
   * Different from above Send*Event(), these methods return true if the
   * event has been posted to the remote process or failed to do that but
   * shouldn't be handled by following event listeners.
   * If you need to check if it's actually posted to the remote process,
   * you can refer aEvent.HasBeenPostedToRemoteProcess().
   */
  bool SendCompositionEvent(mozilla::WidgetCompositionEvent& aEvent);

  bool SendSelectionEvent(mozilla::WidgetSelectionEvent& aEvent);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool SendHandleTap(
      TapType aType, const LayoutDevicePoint& aPoint, Modifiers aModifiers,
      const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId);

  PFilePickerParent* AllocPFilePickerParent(const nsString& aTitle,
                                            const int16_t& aMode);

  bool DeallocPFilePickerParent(PFilePickerParent* actor);

  mozilla::ipc::IPCResult RecvIndexedDBPermissionRequest(
      nsIPrincipal* aPrincipal, IndexedDBPermissionRequestResolver&& aResolve);

  bool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

  void StartPersistence(CanonicalBrowsingContext* aContext,
                        nsIWebBrowserPersistDocumentReceiver* aRecv,
                        ErrorResult& aRv);

  bool HandleQueryContentEvent(mozilla::WidgetQueryContentEvent& aEvent);

  bool SendInsertText(const nsString& aStringToInsert);

  bool SendPasteTransferable(const IPCDataTransfer& aDataTransfer,
                             const bool& aIsPrivateData,
                             nsIPrincipal* aRequestingPrincipal,
                             const nsContentPolicyType& aContentPolicyType);

  // Helper for transforming a point
  LayoutDeviceIntPoint TransformPoint(
      const LayoutDeviceIntPoint& aPoint,
      const LayoutDeviceToLayoutDeviceMatrix4x4& aMatrix);
  LayoutDevicePoint TransformPoint(
      const LayoutDevicePoint& aPoint,
      const LayoutDeviceToLayoutDeviceMatrix4x4& aMatrix);

  // Transform a coordinate from the parent process coordinate space to the
  // child process coordinate space.
  LayoutDeviceIntPoint TransformParentToChild(
      const LayoutDeviceIntPoint& aPoint);
  LayoutDevicePoint TransformParentToChild(const LayoutDevicePoint& aPoint);

  // Transform a coordinate from the child process coordinate space to the
  // parent process coordinate space.
  LayoutDeviceIntPoint TransformChildToParent(
      const LayoutDeviceIntPoint& aPoint);
  LayoutDevicePoint TransformChildToParent(const LayoutDevicePoint& aPoint);
  LayoutDeviceIntRect TransformChildToParent(const LayoutDeviceIntRect& aRect);

  // Returns the matrix that transforms event coordinates from the coordinate
  // space of the child process to the coordinate space of the parent process.
  LayoutDeviceToLayoutDeviceMatrix4x4 GetChildToParentConversionMatrix();

  void SetChildToParentConversionMatrix(
      const Maybe<LayoutDeviceToLayoutDeviceMatrix4x4>& aMatrix,
      const ScreenRect& aRemoteDocumentRect);

  // Returns the offset from the origin of our frameloader's nearest widget to
  // the origin of its layout frame. This offset is used to translate event
  // coordinates relative to the PuppetWidget origin in the child process.
  //
  // GOING AWAY. PLEASE AVOID ADDING CALLERS. Use the above tranformation
  // methods instead.
  LayoutDeviceIntPoint GetChildProcessOffset();

  // Returns the offset from the on-screen origin of our top-level window's
  // widget (including window decorations) to the origin of our frameloader's
  // nearest widget. This offset is used to translate coordinates from the
  // PuppetWidget's origin to absolute screen coordinates in the child.
  LayoutDeviceIntPoint GetClientOffset();

  void StopIMEStateManagement();

  PPaymentRequestParent* AllocPPaymentRequestParent();

  bool DeallocPPaymentRequestParent(PPaymentRequestParent* aActor);

  bool SendLoadRemoteScript(const nsString& aURL,
                            const bool& aRunInGlobalScope);

  void LayerTreeUpdate(const LayersObserverEpoch& aEpoch, bool aActive);

  mozilla::ipc::IPCResult RecvInvokeDragSession(
      nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction,
      Maybe<Shmem>&& aVisualDnDData, const uint32_t& aStride,
      const gfx::SurfaceFormat& aFormat, const LayoutDeviceIntRect& aDragRect,
      nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
      const CookieJarSettingsArgs& aCookieJarSettingsArgs,
      const MaybeDiscarded<WindowContext>& aSourceWindowContext);

  void AddInitialDnDDataTo(DataTransfer* aDataTransfer,
                           nsIPrincipal** aPrincipal);

  bool TakeDragVisualization(RefPtr<mozilla::gfx::SourceSurface>& aSurface,
                             LayoutDeviceIntRect* aDragRect);

  mozilla::ipc::IPCResult RecvEnsureLayersConnected(
      CompositorOptions* aCompositorOptions);

  // LiveResizeListener implementation
  void LiveResizeStarted() override;
  void LiveResizeStopped() override;

  void SetReadyToHandleInputEvents() { mIsReadyToHandleInputEvents = true; }
  bool IsReadyToHandleInputEvents() { return mIsReadyToHandleInputEvents; }

  void NavigateByKey(bool aForward, bool aForDocumentNavigation);

  bool GetDocShellIsActive();
  void SetDocShellIsActive(bool aDocShellIsActive);

  bool GetHasPresented();
  bool GetHasLayers();
  bool GetRenderLayers();
  void SetRenderLayers(bool aRenderLayers);
  void PreserveLayers(bool aPreserveLayers);
  void NotifyResolutionChanged();

  bool CanCancelContentJS(nsIRemoteTab::NavigationType aNavigationType,
                          int32_t aNavigationIndex,
                          nsIURI* aNavigationURI) const;

  // Called when the BrowserParent is being destroyed or entering bfcache.
  void Deactivated();

 protected:
  friend BrowserBridgeParent;
  friend BrowserHost;

  void SetBrowserBridgeParent(BrowserBridgeParent* aBrowser);
  void SetBrowserHost(BrowserHost* aBrowser);

  bool ReceiveMessage(
      const nsString& aMessage, bool aSync, ipc::StructuredCloneData* aData,
      nsTArray<ipc::StructuredCloneData>* aJSONRetVal = nullptr);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  mozilla::ipc::IPCResult RecvRemoteIsReadyToHandleInputEvents();

  mozilla::ipc::IPCResult RecvPaintWhileInterruptingJSNoOp(
      const LayersObserverEpoch& aEpoch);

  mozilla::ipc::IPCResult RecvSetDimensions(
      const uint32_t& aFlags, const int32_t& aX, const int32_t& aY,
      const int32_t& aCx, const int32_t& aCy, const double& aScale);

  mozilla::ipc::IPCResult RecvShowCanvasPermissionPrompt(
      const nsCString& aOrigin, const bool& aHideDoorHanger);

  mozilla::ipc::IPCResult RecvSetSystemFont(const nsCString& aFontName);
  mozilla::ipc::IPCResult RecvGetSystemFont(nsCString* aFontName);

  mozilla::ipc::IPCResult RecvVisitURI(nsIURI* aURI, nsIURI* aLastVisitedURI,
                                       const uint32_t& aFlags);

  mozilla::ipc::IPCResult RecvQueryVisitedState(
      nsTArray<RefPtr<nsIURI>>&& aURIs);

  mozilla::ipc::IPCResult RecvMaybeFireEmbedderLoadEvents(
      EmbedderElementEventType aFireEventAtEmbeddingElement);

  mozilla::ipc::IPCResult RecvRequestPointerLock(
      RequestPointerLockResolver&& aResolve);
  mozilla::ipc::IPCResult RecvReleasePointerLock();

  mozilla::ipc::IPCResult RecvRequestPointerCapture(
      const uint32_t& aPointerId, RequestPointerCaptureResolver&& aResolve);
  mozilla::ipc::IPCResult RecvReleasePointerCapture(const uint32_t& aPointerId);

  mozilla::ipc::IPCResult RecvShowDynamicToolbar();

 private:
  void SuppressDisplayport(bool aEnabled);

  void DestroyInternal();

  void SetRenderLayersInternal(bool aEnabled);

  already_AddRefed<nsFrameLoader> GetFrameLoader(
      bool aUseCachedFrameLoaderAfterDestroy = false) const;

  void TryCacheDPIAndScale();

  bool AsyncPanZoomEnabled() const;

  // Update state prior to routing an APZ-aware event to the child process.
  // |aOutTargetGuid| will contain the identifier
  // of the APZC instance that handled the event. aOutTargetGuid may be null.
  // |aOutInputBlockId| will contain the identifier of the input block
  // that this event was added to, if there was one. aOutInputBlockId may be
  // null. |aOutApzResponse| will contain the response that the APZ gave when
  // processing the input block; this is used for generating appropriate
  // pointercancel events.
  void ApzAwareEventRoutingToChild(ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId,
                                   nsEventStatus* aOutApzResponse);

  // When dropping links we perform a roundtrip from
  // Parent (SendRealDragEvent) -> Child -> Parent (RecvDropLinks)
  // and have to ensure that the child did not modify links to be loaded.
  bool QueryDropLinksForVerification();

  void UnlockNativePointer();

  void UpdateNativePointerLockCenter(nsIWidget* aWidget);

 private:
  // This is used when APZ needs to find the BrowserParent associated with a
  // layer to dispatch events.
  typedef nsTHashMap<nsUint64HashKey, BrowserParent*> LayerToBrowserParentTable;
  static LayerToBrowserParentTable* sLayerToBrowserParentTable;

  static void AddBrowserParentToTable(layers::LayersId aLayersId,
                                      BrowserParent* aBrowserParent);

  static void RemoveBrowserParentFromTable(layers::LayersId aLayersId);

  // Keeps track of which BrowserParent has keyboard focus.
  // If nullptr, the parent process has focus.
  // Use UpdateFocus() to manage.
  static BrowserParent* sFocus;

  // Keeps track of which top-level BrowserParent the keyboard focus is under.
  // If nullptr, the parent process has focus.
  // Use SetTopLevelWebFocus and UnsetTopLevelWebFocus to manage.
  static BrowserParent* sTopLevelWebFocus;

  // Setter for sTopLevelWebFocus
  static void SetTopLevelWebFocus(BrowserParent* aBrowserParent);

  // Unsetter for sTopLevelWebFocus; only unsets if argument matches
  // current sTopLevelWebFocus. Use UnsetTopLevelWebFocusAll() to
  // unset regardless of current value.
  static void UnsetTopLevelWebFocus(BrowserParent* aBrowserParent);

  // Recomputes sFocus and returns it.
  static BrowserParent* UpdateFocus();

  // Keeps track of which BrowserParent the real mouse event is sent to.
  static BrowserParent* sLastMouseRemoteTarget;

  // Unsetter for LastMouseRemoteTarget; only unsets if argument matches
  // current sLastMouseRemoteTarget.
  static void UnsetLastMouseRemoteTarget(BrowserParent* aBrowserParent);

  struct APZData {
    bool operator==(const APZData& aOther) {
      return aOther.guid == guid && aOther.blockId == blockId &&
             aOther.apzResponse == apzResponse;
    }

    bool operator!=(const APZData& aOther) { return !(*this == aOther); }

    ScrollableLayerGuid guid;
    uint64_t blockId;
    nsEventStatus apzResponse;
  };
  void SendRealTouchMoveEvent(WidgetTouchEvent& aEvent, APZData& aAPZData,
                              uint32_t aConsecutiveTouchMoveCount);

  void UpdateVsyncParentVsyncSource();

 public:
  // Unsets sTopLevelWebFocus regardless of its current value.
  static void UnsetTopLevelWebFocusAll();

  // Recomputes focus when the BrowsingContext tree changes in a
  // way that potentially invalidates the sFocus.
  static void UpdateFocusFromBrowsingContext();

 private:
  TabId mTabId;

  RefPtr<ContentParent> mManager;
  // The root browsing context loaded in this BrowserParent.
  RefPtr<CanonicalBrowsingContext> mBrowsingContext;
  nsCOMPtr<nsILoadContext> mLoadContext;
  RefPtr<Element> mFrameElement;
  nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;
  // We keep a strong reference to the frameloader after we've sent the
  // Destroy message and before we've received __delete__. This allows us to
  // dispatch message manager messages during this time.
  RefPtr<nsFrameLoader> mFrameLoader;
  uint32_t mChromeFlags;

  // Pointer back to BrowserBridgeParent if there is one associated with
  // this BrowserParent. This is non-owning to avoid cycles and is managed
  // by the BrowserBridgeParent instance, which has the strong reference
  // to this BrowserParent.
  BrowserBridgeParent* mBrowserBridgeParent;
  // Pointer to the BrowserHost that owns us, if any. This is mutually
  // exclusive with mBrowserBridgeParent, and one is guaranteed to be
  // non-null.
  BrowserHost* mBrowserHost;

  ContentCacheInParent mContentCache;

  layout::RemoteLayerTreeOwner mRemoteLayerTreeOwner;
  LayersObserverEpoch mLayerTreeEpoch;

  Maybe<LayoutDeviceToLayoutDeviceMatrix4x4> mChildToParentConversionMatrix;
  Maybe<ScreenRect> mRemoteDocumentRect;

  nsIntRect mRect;
  ScreenIntSize mDimensions;
  hal::ScreenOrientation mOrientation;
  float mDPI;
  int32_t mRounding;
  CSSToLayoutDeviceScale mDefaultScale;
  bool mUpdatedDimensions;
  nsSizeMode mSizeMode;
  LayoutDeviceIntPoint mClientOffset;
  LayoutDeviceIntPoint mChromeOffset;

  // When loading a new tab or window via window.open, the child is
  // responsible for loading the URL it wants into the new BrowserChild. When
  // the parent receives the CreateWindow message, though, it sends a LoadURL
  // message, usually for about:blank. It's important for the about:blank load
  // to get processed because the Firefox frontend expects every new window to
  // immediately start loading something (see bug 1123090). However, we want
  // the child to process the LoadURL message before it returns from
  // ProvideWindow so that the URL sent from the parent doesn't override the
  // child's URL. This is not possible using our IPC mechanisms. To solve the
  // problem, we skip sending the LoadURL message in the parent and instead
  // return the URL as a result from CreateWindow. The child simulates
  // receiving a LoadURL message before returning from ProvideWindow.
  //
  // The mCreatingWindow flag is set while dispatching CreateWindow. During
  // that time, any LoadURL calls are skipped.
  bool mCreatingWindow;

  // When loading a new tab or window via window.open, we want to ensure that
  // frame scripts for that tab are loaded before any scripts start to run in
  // the window. We can't load the frame scripts the normal way, using
  // separate IPC messages, since they won't be processed by the child until
  // returning to the event loop, which is too late. Instead, we queue up
  // frame scripts that we intend to load and send them as part of the
  // CreateWindow response. Then BrowserChild loads them immediately.
  nsTArray<FrameScriptInfo> mDelayedFrameScripts;

  // Cached cursor setting from BrowserChild.  When the cursor is over the
  // tab, it should take this appearance.
  nsIWidget::Cursor mCursor;

  nsTArray<nsString> mVerifyDropLinks;

  RefPtr<VsyncParent> mVsyncParent;

#ifdef DEBUG
  int32_t mActiveSupressDisplayportCount = 0;
#endif

  // When true, we've initiated normal shutdown and notified our managing
  // PContent.
  bool mMarkedDestroying : 1;
  // When true, the BrowserParent is invalid and we should not send IPC
  // messages anymore.
  bool mIsDestroyed : 1;
  // True if the cursor changes from the BrowserChild should change the widget
  // cursor.  This happens whenever the cursor is in the remote target's
  // region.
  bool mRemoteTargetSetsCursor : 1;

  // If this flag is set, then the tab's layers will be preserved even when
  // the tab's docshell is inactive.
  bool mIsPreservingLayers : 1;

  // Holds the most recent value passed to the RenderLayers function. This
  // does not necessarily mean that the layers have finished rendering
  // and have uploaded - for that, use mHasLayers.
  bool mRenderLayers : 1;

  // True if the compositor has reported that the BrowserChild has uploaded
  // layers.
  bool mHasLayers : 1;

  // True if this BrowserParent has had its layer tree sent to the compositor
  // at least once.
  bool mHasPresented : 1;

  // True when the remote browser is created and ready to handle input events.
  bool mIsReadyToHandleInputEvents : 1;

  // True if we suppress the eMouseEnterIntoWidget event due to the
  // BrowserChild was not ready to handle it. We will resend it when the next
  // time we fire a mouse event and the BrowserChild is ready.
  bool mIsMouseEnterIntoWidgetEventSuppressed : 1;

  // True after RecvLockNativePointer has been called and until
  // UnlockNativePointer has been called.
  bool mLockedNativePointer : 1;

  // True between ShowTooltip and HideTooltip messages.
  bool mShowingTooltip : 1;
};

struct MOZ_STACK_CLASS BrowserParent::AutoUseNewTab final {
 public:
  explicit AutoUseNewTab(BrowserParent* aNewTab) : mNewTab(aNewTab) {
    MOZ_ASSERT(!aNewTab->mCreatingWindow);
    aNewTab->mCreatingWindow = true;
  }

  ~AutoUseNewTab() { mNewTab->mCreatingWindow = false; }

 private:
  RefPtr<BrowserParent> mNewTab;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BrowserParent_h

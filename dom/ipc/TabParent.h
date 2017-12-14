/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "js/TypeDecls.h"
#include "LiveResizeListener.h"
#include "mozilla/ContentCache.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/PFilePickerParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/File.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Move.h"
#include "nsCOMPtr.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMEventListener.h"
#include "nsIKeyEventInPluginCallback.h"
#include "nsISecureBrowserUI.h"
#include "nsITabParent.h"
#include "nsIWebBrowserPersistable.h"
#include "nsIXULBrowserWindow.h"
#include "nsRefreshDriver.h"
#include "nsWeakReference.h"
#include "Units.h"
#include "nsIWidget.h"

class nsFrameLoader;
class nsIFrameLoader;
class nsIContent;
class nsIPrincipal;
class nsIURI;
class nsILoadContext;
class nsIDocShell;

namespace mozilla {

namespace a11y {
class DocAccessibleParent;
}

namespace jsipc {
class CpowHolder;
} // namespace jsipc

namespace layers {
struct TextureFactoryIdentifier;
} // namespace layers

namespace layout {
class RenderFrameParent;
} // namespace layout

namespace widget {
struct IMENotification;
} // namespace widget

namespace gfx {
class SourceSurface;
class DataSourceSurface;
} // namespace gfx

namespace dom {

class ClonedMessageData;
class nsIContentParent;
class Element;
class DataTransfer;

namespace ipc {
class StructuredCloneData;
} // ipc namespace

class TabParent final : public PBrowserParent
                      , public nsIDOMEventListener
                      , public nsITabParent
                      , public nsIAuthPromptProvider
                      , public nsISecureBrowserUI
                      , public nsIKeyEventInPluginCallback
                      , public nsSupportsWeakReference
                      , public TabContext
                      , public nsIWebBrowserPersistable
                      , public LiveResizeListener
{
  typedef mozilla::dom::ClonedMessageData ClonedMessageData;

  virtual ~TabParent();

public:
  // Helper class for ContentParent::RecvCreateWindow.
  struct AutoUseNewTab;

  // nsITabParent
  NS_DECL_NSITABPARENT
  // nsIDOMEventListener interfaces
  NS_DECL_NSIDOMEVENTLISTENER

  TabParent(nsIContentParent* aManager,
            const TabId& aTabId,
            const TabContext& aContext,
            uint32_t aChromeFlags);

  Element* GetOwnerElement() const { return mFrameElement; }
  already_AddRefed<nsPIDOMWindowOuter> GetParentWindowOuter();

  void SetOwnerElement(Element* aElement);

  void CacheFrameLoader(nsFrameLoader* aFrameLoader);

  nsIBrowserDOMWindow *GetBrowserDOMWindow() const { return mBrowserDOMWindow; }

  void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow)
  {
    mBrowserDOMWindow = aBrowserDOMWindow;
  }

  void SetHasContentOpener(bool aHasContentOpener);

  void SwapFrameScriptsFrom(nsTArray<FrameScriptInfo>& aFrameScripts)
  {
    aFrameScripts.SwapElements(mDelayedFrameScripts);
  }

  already_AddRefed<nsILoadContext> GetLoadContext();

  already_AddRefed<nsIWidget> GetTopLevelWidget();

  nsIXULBrowserWindow* GetXULBrowserWindow();

  void Destroy();

  void RemoveWindowListeners();

  void AddWindowListeners();

  virtual mozilla::ipc::IPCResult RecvMoveFocus(const bool& aForward,
                                                const bool& aForDocumentNavigation) override;

  virtual mozilla::ipc::IPCResult RecvSizeShellTo(const uint32_t& aFlags,
                                                  const int32_t& aWidth,
                                                  const int32_t& aHeight,
                                                  const int32_t& aShellItemWidth,
                                                  const int32_t& aShellItemHeight) override;

  virtual mozilla::ipc::IPCResult RecvDropLinks(nsTArray<nsString>&& aLinks) override;

  virtual mozilla::ipc::IPCResult RecvEvent(const RemoteDOMEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult RecvReplyKeyEvent(const WidgetKeyboardEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvAccessKeyNotHandled(const WidgetKeyboardEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvSetHasBeforeUnload(const bool& aHasBeforeUnload) override;

  virtual mozilla::ipc::IPCResult
  RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                             PRenderFrameParent* aRenderFrame,
                             const nsString& aURL,
                             const nsString& aName,
                             const nsString& aFeatures,
                             BrowserFrameOpenWindowResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult
  RecvSyncMessage(const nsString& aMessage,
                  const ClonedMessageData& aData,
                  InfallibleTArray<CpowEntry>&& aCpows,
                  const IPC::Principal& aPrincipal,
                  nsTArray<ipc::StructuredCloneData>* aRetVal) override;

  virtual mozilla::ipc::IPCResult
  RecvRpcMessage(const nsString& aMessage,
                 const ClonedMessageData& aData,
                 InfallibleTArray<CpowEntry>&& aCpows,
                 const IPC::Principal& aPrincipal,
                 nsTArray<ipc::StructuredCloneData>* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvAsyncMessage(const nsString& aMessage,
                                                   InfallibleTArray<CpowEntry>&& aCpows,
                                                   const IPC::Principal& aPrincipal,
                                                   const ClonedMessageData& aData) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMEFocus(const ContentCache& aContentCache,
                     const widget::IMENotification& aEventMessage,
                     NotifyIMEFocusResolver&& aResolve) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMETextChange(const ContentCache& aContentCache,
                          const widget::IMENotification& aEventMessage) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMECompositionUpdate(const ContentCache& aContentCache,
                                 const widget::IMENotification& aEventMessage) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMESelection(const ContentCache& aContentCache,
                         const widget::IMENotification& aEventMessage) override;

  virtual mozilla::ipc::IPCResult
  RecvUpdateContentCache(const ContentCache& aContentCache) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMEMouseButtonEvent(const widget::IMENotification& aEventMessage,
                                bool* aConsumedByIME) override;

  virtual mozilla::ipc::IPCResult
  RecvNotifyIMEPositionChange(const ContentCache& aContentCache,
                              const widget::IMENotification& aEventMessage) override;

  virtual mozilla::ipc::IPCResult
  RecvOnEventNeedingAckHandled(const EventMessage& aMessage) override;

  virtual mozilla::ipc::IPCResult
  RecvRequestIMEToCommitComposition(const bool& aCancel,
                                    bool* aIsCommitted,
                                    nsString* aCommittedString) override;

  virtual mozilla::ipc::IPCResult
  RecvStartPluginIME(const WidgetKeyboardEvent& aKeyboardEvent,
                     const int32_t& aPanelX,
                     const int32_t& aPanelY,
                     nsString* aCommitted) override;

  virtual mozilla::ipc::IPCResult RecvSetPluginFocused(const bool& aFocused) override;

  virtual mozilla::ipc::IPCResult RecvSetCandidateWindowForPlugin(
    const widget::CandidateWindowPosition& aPosition) override;

  virtual mozilla::ipc::IPCResult
  RecvDefaultProcOfPluginEvent(const WidgetPluginEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult RecvGetInputContext(int32_t* aIMEEnabled,
                                                      int32_t* aIMEOpen) override;

  virtual mozilla::ipc::IPCResult RecvSetInputContext(const int32_t& aIMEEnabled,
                                                      const int32_t& aIMEOpen,
                                                      const nsString& aType,
                                                      const nsString& aInputmode,
                                                      const nsString& aActionHint,
                                                      const bool& aInPrivateBrowsing,
                                                      const int32_t& aCause,
                                                      const int32_t& aFocusChange) override;


  // See nsIKeyEventInPluginCallback
  virtual void HandledWindowedPluginKeyEvent(
                 const NativeEventData& aKeyEventData,
                 bool aIsConsumed) override;

  virtual mozilla::ipc::IPCResult RecvOnWindowedPluginKeyEvent(
    const NativeEventData& aKeyEventData) override;

  virtual mozilla::ipc::IPCResult RecvRequestFocus(const bool& aCanRaise) override;

  virtual mozilla::ipc::IPCResult RecvLookUpDictionary(
    const nsString& aText,
    nsTArray<mozilla::FontRange>&& aFontRangeArray,
    const bool& aIsVertical,
    const LayoutDeviceIntPoint& aPoint) override;

  virtual mozilla::ipc::IPCResult
  RecvEnableDisableCommands(const nsString& aAction,
                            nsTArray<nsCString>&& aEnabledCommands,
                            nsTArray<nsCString>&& aDisabledCommands) override;

  virtual mozilla::ipc::IPCResult
  RecvSetCursor(const uint32_t& aValue, const bool& aForce) override;

  virtual mozilla::ipc::IPCResult RecvSetCustomCursor(const nsCString& aUri,
                                                      const uint32_t& aWidth,
                                                      const uint32_t& aHeight,
                                                      const uint32_t& aStride,
                                                      const uint8_t& aFormat,
                                                      const uint32_t& aHotspotX,
                                                      const uint32_t& aHotspotY,
                                                      const bool& aForce) override;

  virtual mozilla::ipc::IPCResult RecvSetStatus(const uint32_t& aType,
                                                const nsString& aStatus) override;

  virtual mozilla::ipc::IPCResult RecvIsParentWindowMainWidgetVisible(bool* aIsVisible) override;

  virtual mozilla::ipc::IPCResult RecvShowTooltip(const uint32_t& aX,
                                                  const uint32_t& aY,
                                                  const nsString& aTooltip,
                                                  const nsString& aDirection) override;

  virtual mozilla::ipc::IPCResult RecvHideTooltip() override;


  virtual mozilla::ipc::IPCResult RecvSetNativeChildOfShareableWindow(const uintptr_t& childWindow) override;

  virtual mozilla::ipc::IPCResult RecvDispatchFocusToTopLevelWindow() override;

  virtual mozilla::ipc::IPCResult RecvRespondStartSwipeEvent(const uint64_t& aInputBlockId,
                                                             const bool& aStartSwipe) override;

  virtual mozilla::ipc::IPCResult
  RecvDispatchWheelEvent(const mozilla::WidgetWheelEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvDispatchMouseEvent(const mozilla::WidgetMouseEvent& aEvent) override;

  virtual mozilla::ipc::IPCResult
  RecvDispatchKeyboardEvent(const mozilla::WidgetKeyboardEvent& aEvent) override;

  virtual PColorPickerParent*
  AllocPColorPickerParent(const nsString& aTitle,
                          const nsString& aInitialColor) override;

  virtual bool
  DeallocPColorPickerParent(PColorPickerParent* aColorPicker) override;

  virtual PDocAccessibleParent*
  AllocPDocAccessibleParent(PDocAccessibleParent*, const uint64_t&,
                            const uint32_t&, const IAccessibleHolder&) override;

  virtual bool DeallocPDocAccessibleParent(PDocAccessibleParent*) override;

  virtual mozilla::ipc::IPCResult
  RecvPDocAccessibleConstructor(PDocAccessibleParent* aDoc,
                                PDocAccessibleParent* aParentDoc,
                                const uint64_t& aParentID,
                                const uint32_t& aMsaaID,
                                const IAccessibleHolder& aDocCOMProxy) override;

  /**
   * Return the top level doc accessible parent for this tab.
   */
  a11y::DocAccessibleParent* GetTopLevelDocAccessible() const;

  void LoadURL(nsIURI* aURI);

  void InitRenderFrame();

  // XXX/cjones: it's not clear what we gain by hiding these
  // message-sending functions under a layer of indirection and
  // eating the return values
  void Show(const ScreenIntSize& aSize, bool aParentIsActive);

  void UpdateDimensions(const nsIntRect& aRect, const ScreenIntSize& aSize);

  DimensionInfo GetDimensionInfo();

  nsresult UpdatePosition();

  void SizeModeChanged(const nsSizeMode& aSizeMode);

  void UIResolutionChanged();

  void ThemeChanged();

  void HandleAccessKey(const WidgetKeyboardEvent& aEvent,
                       nsTArray<uint32_t>& aCharCodes);

  void Activate();

  void Deactivate();

  bool MapEventCoordinatesForChildProcess(mozilla::WidgetEvent* aEvent);

  void MapEventCoordinatesForChildProcess(const LayoutDeviceIntPoint& aOffset,
                                          mozilla::WidgetEvent* aEvent);

  LayoutDeviceToCSSScale GetLayoutDeviceToCSSScale();

  virtual mozilla::ipc::IPCResult
  RecvRequestNativeKeyBindings(
    const uint32_t& aType,
    const mozilla::WidgetKeyboardEvent& aEvent,
    nsTArray<mozilla::CommandInt>* aCommands) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeKeyEvent(const int32_t& aNativeKeyboardLayout,
                               const int32_t& aNativeKeyCode,
                               const uint32_t& aModifierFlags,
                               const nsString& aCharacters,
                               const nsString& aUnmodifiedCharacters,
                               const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeMouseEvent(const LayoutDeviceIntPoint& aPoint,
                                 const uint32_t& aNativeMessage,
                                 const uint32_t& aModifierFlags,
                                 const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeMouseMove(const LayoutDeviceIntPoint& aPoint,
                                const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeMouseScrollEvent(const LayoutDeviceIntPoint& aPoint,
                                       const uint32_t& aNativeMessage,
                                       const double& aDeltaX,
                                       const double& aDeltaY,
                                       const double& aDeltaZ,
                                       const uint32_t& aModifierFlags,
                                       const uint32_t& aAdditionalFlags,
                                       const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeTouchPoint(const uint32_t& aPointerId,
                                 const TouchPointerState& aPointerState,
                                 const LayoutDeviceIntPoint& aPoint,
                                 const double& aPointerPressure,
                                 const uint32_t& aPointerOrientation,
                                 const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvSynthesizeNativeTouchTap(const LayoutDeviceIntPoint& aPoint,
                               const bool& aLongTap,
                               const uint64_t& aObserverId) override;

  virtual mozilla::ipc::IPCResult
  RecvClearNativeTouchSequence(const uint64_t& aObserverId) override;

  void SendMouseEvent(const nsAString& aType, float aX, float aY,
                      int32_t aButton, int32_t aClickCount,
                      int32_t aModifiers, bool aIgnoreRootScrollFrame);

  void SendKeyEvent(const nsAString& aType, int32_t aKeyCode,
                    int32_t aCharCode, int32_t aModifiers,
                    bool aPreventDefault);

  /**
   * The following Send*Event() marks aEvent as posted to remote process if
   * it succeeded.  So, you can check the result with
   * aEvent.HasBeenPostedToRemoteProcess().
   */
  void SendRealMouseEvent(WidgetMouseEvent& aEvent);

  void SendRealDragEvent(WidgetDragEvent& aEvent,
                         uint32_t aDragAction,
                         uint32_t aDropEffect);

  void SendMouseWheelEvent(WidgetWheelEvent& aEvent);

  void SendRealKeyEvent(WidgetKeyboardEvent& aEvent);

  void SendRealTouchEvent(WidgetTouchEvent& aEvent);

  void SendPluginEvent(WidgetPluginEvent& aEvent);

  /**
   * Different from above Send*Event(), these methods return true if the
   * event has been posted to the remote process or failed to do that but
   * shouldn't be handled by following event listeners.
   * If you need to check if it's actually posted to the remote process,
   * you can refer aEvent.HasBeenPostedToRemoteProcess().
   */
  bool SendCompositionEvent(mozilla::WidgetCompositionEvent& aEvent);

  bool SendSelectionEvent(mozilla::WidgetSelectionEvent& aEvent);

  bool SendHandleTap(TapType aType,
                     const LayoutDevicePoint& aPoint,
                     Modifiers aModifiers,
                     const ScrollableLayerGuid& aGuid,
                     uint64_t aInputBlockId);

  virtual PDocumentRendererParent*
  AllocPDocumentRendererParent(const nsRect& documentRect,
                               const gfx::Matrix& transform,
                               const nsString& bgcolor,
                               const uint32_t& renderFlags,
                               const bool& flushLayout,
                               const nsIntSize& renderSize) override;

  virtual bool
  DeallocPDocumentRendererParent(PDocumentRendererParent* actor) override;

  virtual PFilePickerParent*
  AllocPFilePickerParent(const nsString& aTitle,
                         const int16_t& aMode) override;

  virtual bool DeallocPFilePickerParent(PFilePickerParent* actor) override;

  virtual PIndexedDBPermissionRequestParent*
  AllocPIndexedDBPermissionRequestParent(const Principal& aPrincipal) override;

  virtual mozilla::ipc::IPCResult
  RecvPIndexedDBPermissionRequestConstructor(
    PIndexedDBPermissionRequestParent* aActor,
    const Principal& aPrincipal)
                                    override;

  virtual bool
  DeallocPIndexedDBPermissionRequestParent(
                                    PIndexedDBPermissionRequestParent* aActor)
                                    override;

  bool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHPROMPTPROVIDER
  NS_DECL_NSISECUREBROWSERUI
  NS_DECL_NSIWEBBROWSERPERSISTABLE

  bool HandleQueryContentEvent(mozilla::WidgetQueryContentEvent& aEvent);

  bool SendPasteTransferable(const IPCDataTransfer& aDataTransfer,
                             const bool& aIsPrivateData,
                             const IPC::Principal& aRequestingPrincipal);

  static TabParent* GetFrom(nsFrameLoader* aFrameLoader);

  static TabParent* GetFrom(nsIFrameLoader* aFrameLoader);

  static TabParent* GetFrom(nsITabParent* aTabParent);

  static TabParent* GetFrom(PBrowserParent* aTabParent);

  static TabParent* GetFrom(nsIContent* aContent);

  static TabId GetTabIdFrom(nsIDocShell* docshell);

  nsIContentParent* Manager() const { return mManager; }

  /**
   * Let managees query if Destroy() is already called so they don't send out
   * messages when the PBrowser actor is being destroyed.
   */
  bool IsDestroyed() const { return mIsDestroyed; }

  // Returns the closest widget for our frameloader's content.
  already_AddRefed<nsIWidget> GetWidget() const;

  // Returns the top-level widget for our frameloader's document.
  already_AddRefed<nsIWidget> GetDocWidget() const;

  const TabId GetTabId() const
  {
    return mTabId;
  }

  // Returns the offset from the origin of our frameloader's nearest widget to
  // the origin of its layout frame. This offset is used to translate event
  // coordinates relative to the PuppetWidget origin in the child process.
  LayoutDeviceIntPoint GetChildProcessOffset();

  // Returns the offset from the on-screen origin of our top-level window's
  // widget (including window decorations) to the origin of our frameloader's
  // nearest widget. This offset is used to translate coordinates from the
  // PuppetWidget's origin to absolute screen coordinates in the child.
  LayoutDeviceIntPoint GetClientOffset();

  LayoutDevicePoint AdjustTapToChildWidget(const LayoutDevicePoint& aPoint);

  /**
   * Native widget remoting protocol for use with windowed plugins with e10s.
   */
  virtual PPluginWidgetParent* AllocPPluginWidgetParent() override;

  virtual bool
  DeallocPPluginWidgetParent(PPluginWidgetParent* aActor) override;

  virtual PPaymentRequestParent*
  AllocPPaymentRequestParent() override;

  virtual bool
  DeallocPPaymentRequestParent(PPaymentRequestParent* aActor) override;

  void SetInitedByParent() { mInitedByParent = true; }

  bool IsInitedByParent() const { return mInitedByParent; }

  bool SendLoadRemoteScript(const nsString& aURL,
                            const bool& aRunInGlobalScope);

  void LayerTreeUpdate(uint64_t aEpoch, bool aActive);

  virtual mozilla::ipc::IPCResult
  RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                        const uint32_t& aAction,
                        const OptionalShmem& aVisualDnDData,
                        const uint32_t& aStride, const uint8_t& aFormat,
                        const LayoutDeviceIntRect& aDragRect) override;

  void AddInitialDnDDataTo(DataTransfer* aDataTransfer);

  bool TakeDragVisualization(RefPtr<mozilla::gfx::SourceSurface>& aSurface,
                             LayoutDeviceIntRect* aDragRect);

  layout::RenderFrameParent* GetRenderFrame();

  bool SetRenderFrame(PRenderFrameParent* aRFParent);
  bool GetRenderFrameInfo(TextureFactoryIdentifier* aTextureFactoryIdentifier,
                          uint64_t* aLayersId);

  mozilla::ipc::IPCResult RecvEnsureLayersConnected(CompositorOptions* aCompositorOptions) override;

  // LiveResizeListener implementation
  void LiveResizeStarted() override;
  void LiveResizeStopped() override;

  void SetReadyToHandleInputEvents() { mIsReadyToHandleInputEvents = true; }
  bool IsReadyToHandleInputEvents() { return mIsReadyToHandleInputEvents; }

protected:
  bool ReceiveMessage(const nsString& aMessage,
                      bool aSync,
                      ipc::StructuredCloneData* aData,
                      mozilla::jsipc::CpowHolder* aCpows,
                      nsIPrincipal* aPrincipal,
                      nsTArray<ipc::StructuredCloneData>* aJSONRetVal = nullptr);

  virtual mozilla::ipc::IPCResult RecvAsyncAuthPrompt(const nsCString& aUri,
                                                      const nsString& aRealm,
                                                      const uint64_t& aCallbackId) override;

  virtual mozilla::ipc::IPCResult Recv__delete__() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  Element* mFrameElement;
  nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;

  virtual PRenderFrameParent* AllocPRenderFrameParent() override;

  virtual bool DeallocPRenderFrameParent(PRenderFrameParent* aFrame) override;

  virtual mozilla::ipc::IPCResult RecvRemotePaintIsReady() override;

  virtual mozilla::ipc::IPCResult RecvRemoteIsReadyToHandleInputEvents() override;

  virtual mozilla::ipc::IPCResult RecvForcePaintNoOp(const uint64_t& aLayerObserverEpoch) override;

  virtual mozilla::ipc::IPCResult RecvSetDimensions(const uint32_t& aFlags,
                                                    const int32_t& aX, const int32_t& aY,
                                                    const int32_t& aCx, const int32_t& aCy) override;

  virtual mozilla::ipc::IPCResult RecvGetTabCount(uint32_t* aValue) override;

  virtual mozilla::ipc::IPCResult RecvShowCanvasPermissionPrompt(const nsCString& aFirstPartyURI) override;

  ContentCacheInParent mContentCache;

  nsIntRect mRect;
  ScreenIntSize mDimensions;
  ScreenOrientationInternal mOrientation;
  float mDPI;
  int32_t mRounding;
  CSSToLayoutDeviceScale mDefaultScale;
  bool mUpdatedDimensions;
  nsSizeMode mSizeMode;
  LayoutDeviceIntPoint mClientOffset;
  LayoutDeviceIntPoint mChromeOffset;

private:
  void DestroyInternal();

  already_AddRefed<nsFrameLoader>
  GetFrameLoader(bool aUseCachedFrameLoaderAfterDestroy = false) const;

  RefPtr<nsIContentParent> mManager;
  void TryCacheDPIAndScale();

  bool AsyncPanZoomEnabled() const;

  // Cached value indicating the docshell active state of the remote browser.
  bool mDocShellIsActive;

  // Update state prior to routing an APZ-aware event to the child process.
  // |aOutTargetGuid| will contain the identifier
  // of the APZC instance that handled the event. aOutTargetGuid may be null.
  // |aOutInputBlockId| will contain the identifier of the input block
  // that this event was added to, if there was one. aOutInputBlockId may be null.
  // |aOutApzResponse| will contain the response that the APZ gave when processing
  // the input block; this is used for generating appropriate pointercancel events.
  void ApzAwareEventRoutingToChild(ScrollableLayerGuid* aOutTargetGuid,
                                   uint64_t* aOutInputBlockId,
                                   nsEventStatus* aOutApzResponse);

  // When true, we've initiated normal shutdown and notified our managing PContent.
  bool mMarkedDestroying;
  // When true, the TabParent is invalid and we should not send IPC messages anymore.
  bool mIsDestroyed;

  uint32_t mChromeFlags;

  nsTArray<nsTArray<IPCDataTransferItem>> mInitialDataTransferItems;

  RefPtr<gfx::DataSourceSurface> mDnDVisualization;
  bool mDragValid;
  LayoutDeviceIntRect mDragRect;

  // When true, the TabParent is initialized without child side's request.
  // When false, the TabParent is initialized by window.open() from child side.
  bool mInitedByParent;

  nsCOMPtr<nsILoadContext> mLoadContext;

  // We keep a strong reference to the frameloader after we've sent the
  // Destroy message and before we've received __delete__. This allows us to
  // dispatch message manager messages during this time.
  RefPtr<nsFrameLoader> mFrameLoader;

  TabId mTabId;

  // When loading a new tab or window via window.open, the child is
  // responsible for loading the URL it wants into the new TabChild. When the
  // parent receives the CreateWindow message, though, it sends a LoadURL
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
  // that time, any LoadURL calls are skipped and the URL is stored in
  // mSkippedURL.
  bool mCreatingWindow;
  nsCString mDelayedURL;

  // When loading a new tab or window via window.open, we want to ensure that
  // frame scripts for that tab are loaded before any scripts start to run in
  // the window. We can't load the frame scripts the normal way, using
  // separate IPC messages, since they won't be processed by the child until
  // returning to the event loop, which is too late. Instead, we queue up
  // frame scripts that we intend to load and send them as part of the
  // CreateWindow response. Then TabChild loads them immediately.
  nsTArray<FrameScriptInfo> mDelayedFrameScripts;

  // Cached cursor setting from TabChild.  When the cursor is over the tab,
  // it should take this appearance.
  nsCursor mCursor;
  nsCOMPtr<imgIContainer> mCustomCursor;
  uint32_t mCustomCursorHotspotX, mCustomCursorHotspotY;

  // True if the cursor changes from the TabChild should change the widget
  // cursor.  This happens whenever the cursor is in the tab's region.
  bool mTabSetsCursor;

  bool mHasContentOpener;

  // When dropping links we perform a roundtrip from
  // Parent (SendRealDragEvent) -> Child -> Parent (RecvDropLinks)
  // and have to ensure that the child did not modify links to be loaded.
  bool QueryDropLinksForVerification();
  nsTArray<nsString> mVerifyDropLinks;

#ifdef DEBUG
  int32_t mActiveSupressDisplayportCount;
#endif

  ShowInfo GetShowInfo();

private:
  // This is used when APZ needs to find the TabParent associated with a layer
  // to dispatch events.
  typedef nsDataHashtable<nsUint64HashKey, TabParent*> LayerToTabParentTable;
  static LayerToTabParentTable* sLayerToTabParentTable;

  static void AddTabParentToTable(uint64_t aLayersId, TabParent* aTabParent);

  static void RemoveTabParentFromTable(uint64_t aLayersId);

  uint64_t mLayerTreeEpoch;

  // If this flag is set, then the tab's layers will be preserved even when
  // the tab's docshell is inactive.
  bool mPreserveLayers;

  // Holds the most recent value passed to the RenderLayers function. This
  // does not necessarily mean that the layers have finished rendering
  // and have uploaded - for that, use mHasLayers.
  bool mRenderLayers;

  // True if the compositor has reported that the TabChild has uploaded
  // layers.
  bool mHasLayers;

  // True if this TabParent has had its layer tree sent to the compositor
  // at least once.
  bool mHasPresented;

  // True if at least one window hosted in the TabChild has added a
  // beforeunload event listener.
  bool mHasBeforeUnload;

  // True when the remote browser is created and ready to handle input events.
  bool mIsReadyToHandleInputEvents;

  // True if we suppress the eMouseEnterIntoWidget event due to the TabChild was
  // not ready to handle it. We will resend it when the next time we fire a
  // mouse event and the TabChild is ready.
  bool mIsMouseEnterIntoWidgetEventSuppressed;

public:
  static TabParent* GetTabParentFromLayersId(uint64_t aLayersId);
};

struct MOZ_STACK_CLASS TabParent::AutoUseNewTab final
{
public:
  AutoUseNewTab(TabParent* aNewTab, nsCString* aURLToLoad)
   : mNewTab(aNewTab), mURLToLoad(aURLToLoad)
  {
    MOZ_ASSERT(!aNewTab->mCreatingWindow);

    aNewTab->mCreatingWindow = true;
    aNewTab->mDelayedURL.Truncate();
  }

  ~AutoUseNewTab()
  {
    mNewTab->mCreatingWindow = false;
    *mURLToLoad = mNewTab->mDelayedURL;
  }

private:
  TabParent* mNewTab;
  nsCString* mURLToLoad;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_tabs_TabParent_h

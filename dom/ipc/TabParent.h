/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "js/TypeDecls.h"
#include "mozilla/ContentCache.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/PFilePickerParent.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/File.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMEventListener.h"
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

namespace jsipc {
class CpowHolder;
} // namespace jsipc

namespace layers {
struct FrameMetrics;
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
struct StructuredCloneData;

class TabParent final : public PBrowserParent
                      , public nsIDOMEventListener
                      , public nsITabParent
                      , public nsIAuthPromptProvider
                      , public nsISecureBrowserUI
                      , public nsSupportsWeakReference
                      , public TabContext
                      , public nsAPostRefreshObserver
                      , public nsIWebBrowserPersistable
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::OwningSerializedStructuredCloneBuffer OwningSerializedStructuredCloneBuffer;

    virtual ~TabParent();

public:
    // nsITabParent
    NS_DECL_NSITABPARENT
    // nsIDOMEventListener interfaces
    NS_DECL_NSIDOMEVENTLISTENER

    TabParent(nsIContentParent* aManager,
              const TabId& aTabId,
              const TabContext& aContext,
              uint32_t aChromeFlags);
    Element* GetOwnerElement() const { return mFrameElement; }
    void SetOwnerElement(Element* aElement);

    void CacheFrameLoader(nsFrameLoader* aFrameLoader);

    /**
     * Get the mozapptype attribute from this TabParent's owner DOM element.
     */
    void GetAppType(nsAString& aOut);

    /**
     * Returns true iff this TabParent's nsIFrameLoader is visible.
     *
     * The frameloader's visibility can be independent of e.g. its docshell's
     * visibility.
     */
    bool IsVisible();

    nsIBrowserDOMWindow *GetBrowserDOMWindow() { return mBrowserDOMWindow; }
    void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow) {
        mBrowserDOMWindow = aBrowserDOMWindow;
    }

    already_AddRefed<nsILoadContext> GetLoadContext();
    already_AddRefed<nsIWidget> GetTopLevelWidget();
    nsIXULBrowserWindow* GetXULBrowserWindow();

    void Destroy();

    void RemoveWindowListeners();
    void AddWindowListeners();
    void DidRefresh() override;

    virtual bool RecvMoveFocus(const bool& aForward,
                               const bool& aForDocumentNavigation) override;
    virtual bool RecvEvent(const RemoteDOMEvent& aEvent) override;
    virtual bool RecvReplyKeyEvent(const WidgetKeyboardEvent& aEvent) override;
    virtual bool RecvDispatchAfterKeyboardEvent(const WidgetKeyboardEvent& aEvent) override;
    virtual bool RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                            const nsString& aURL,
                                            const nsString& aName,
                                            const nsString& aFeatures,
                                            bool* aOutWindowOpened) override;
    virtual bool RecvCreateWindow(PBrowserParent* aOpener,
                                  const uint32_t& aChromeFlags,
                                  const bool& aCalledFromJS,
                                  const bool& aPositionSpecified,
                                  const bool& aSizeSpecified,
                                  const nsString& aURI,
                                  const nsString& aName,
                                  const nsCString& aFeatures,
                                  const nsString& aBaseURI,
                                  nsresult* aResult,
                                  bool* aWindowIsNew,
                                  InfallibleTArray<FrameScriptInfo>* aFrameScripts,
                                  nsCString* aURLToLoad) override;
    virtual bool RecvSyncMessage(const nsString& aMessage,
                                 const ClonedMessageData& aData,
                                 InfallibleTArray<CpowEntry>&& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 nsTArray<OwningSerializedStructuredCloneBuffer>* aRetVal) override;
    virtual bool RecvRpcMessage(const nsString& aMessage,
                                const ClonedMessageData& aData,
                                InfallibleTArray<CpowEntry>&& aCpows,
                                const IPC::Principal& aPrincipal,
                                nsTArray<OwningSerializedStructuredCloneBuffer>* aRetVal) override;
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal) override;
    virtual bool RecvNotifyIMEFocus(const ContentCache& aContentCache,
                                    const widget::IMENotification& aEventMessage,
                                    nsIMEUpdatePreference* aPreference)
                                      override;
    virtual bool RecvNotifyIMETextChange(const ContentCache& aContentCache,
                                         const widget::IMENotification& aEventMessage) override;
    virtual bool RecvNotifyIMECompositionUpdate(const ContentCache& aContentCache,
                                                const widget::IMENotification& aEventMessage) override;
    virtual bool RecvNotifyIMESelection(const ContentCache& aContentCache,
                                        const widget::IMENotification& aEventMessage) override;
    virtual bool RecvUpdateContentCache(const ContentCache& aContentCache) override;
    virtual bool RecvNotifyIMEMouseButtonEvent(const widget::IMENotification& aEventMessage,
                                               bool* aConsumedByIME) override;
    virtual bool RecvNotifyIMEPositionChange(const ContentCache& aContentCache,
                                             const widget::IMENotification& aEventMessage) override;
    virtual bool RecvOnEventNeedingAckHandled(const EventMessage& aMessage) override;
    virtual bool RecvEndIMEComposition(const bool& aCancel,
                                       bool* aNoCompositionEvent,
                                       nsString* aComposition) override;
    virtual bool RecvStartPluginIME(const WidgetKeyboardEvent& aKeyboardEvent,
                                    const int32_t& aPanelX,
                                    const int32_t& aPanelY,
                                    nsString* aCommitted) override;
    virtual bool RecvSetPluginFocused(const bool& aFocused) override;
    virtual bool RecvGetInputContext(int32_t* aIMEEnabled,
                                     int32_t* aIMEOpen,
                                     intptr_t* aNativeIMEContext) override;
    virtual bool RecvSetInputContext(const int32_t& aIMEEnabled,
                                     const int32_t& aIMEOpen,
                                     const nsString& aType,
                                     const nsString& aInputmode,
                                     const nsString& aActionHint,
                                     const int32_t& aCause,
                                     const int32_t& aFocusChange) override;
    virtual bool RecvRequestFocus(const bool& aCanRaise) override;
    virtual bool RecvEnableDisableCommands(const nsString& aAction,
                                           nsTArray<nsCString>&& aEnabledCommands,
                                           nsTArray<nsCString>&& aDisabledCommands) override;
    virtual bool RecvSetCursor(const uint32_t& aValue, const bool& aForce) override;
    virtual bool RecvSetCustomCursor(const nsCString& aUri,
                                     const uint32_t& aWidth,
                                     const uint32_t& aHeight,
                                     const uint32_t& aStride,
                                     const uint8_t& aFormat,
                                     const uint32_t& aHotspotX,
                                     const uint32_t& aHotspotY,
                                     const bool& aForce) override;
    virtual bool RecvSetBackgroundColor(const nscolor& aValue) override;
    virtual bool RecvSetStatus(const uint32_t& aType, const nsString& aStatus) override;
    virtual bool RecvIsParentWindowMainWidgetVisible(bool* aIsVisible) override;
    virtual bool RecvShowTooltip(const uint32_t& aX, const uint32_t& aY, const nsString& aTooltip) override;
    virtual bool RecvHideTooltip() override;
    virtual bool RecvGetTabOffset(LayoutDeviceIntPoint* aPoint) override;
    virtual bool RecvGetDPI(float* aValue) override;
    virtual bool RecvGetDefaultScale(double* aValue) override;
    virtual bool RecvGetMaxTouchPoints(uint32_t* aTouchPoints) override;
    virtual bool RecvGetWidgetNativeData(WindowsHandle* aValue) override;
    virtual bool RecvSetNativeChildOfShareableWindow(const uintptr_t& childWindow) override;
    virtual bool RecvDispatchFocusToTopLevelWindow() override;
    virtual bool RecvZoomToRect(const uint32_t& aPresShellId,
                                const ViewID& aViewId,
                                const CSSRect& aRect) override;
    virtual bool RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                                           const ViewID& aViewId,
                                           const MaybeZoomConstraints& aConstraints) override;
    virtual bool RecvRespondStartSwipeEvent(const uint64_t& aInputBlockId,
                                            const bool& aStartSwipe) override;
    virtual bool RecvContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                               const uint64_t& aInputBlockId,
                                               const bool& aPreventDefault) override;
    virtual bool RecvSetTargetAPZC(const uint64_t& aInputBlockId,
                                   nsTArray<ScrollableLayerGuid>&& aTargets) override;
    virtual bool RecvSetAllowedTouchBehavior(const uint64_t& aInputBlockId,
                                             nsTArray<TouchBehaviorFlags>&& aTargets) override;
    virtual bool RecvDispatchWheelEvent(const mozilla::WidgetWheelEvent& aEvent) override;
    virtual bool RecvDispatchMouseEvent(const mozilla::WidgetMouseEvent& aEvent) override;
    virtual bool RecvDispatchKeyboardEvent(const mozilla::WidgetKeyboardEvent& aEvent) override;

    virtual PColorPickerParent*
    AllocPColorPickerParent(const nsString& aTitle, const nsString& aInitialColor) override;
    virtual bool DeallocPColorPickerParent(PColorPickerParent* aColorPicker) override;

    virtual PDocAccessibleParent*
    AllocPDocAccessibleParent(PDocAccessibleParent*, const uint64_t&) override;
    virtual bool DeallocPDocAccessibleParent(PDocAccessibleParent*) override;
    virtual bool
    RecvPDocAccessibleConstructor(PDocAccessibleParent* aDoc,
                                  PDocAccessibleParent* aParentDoc,
                                  const uint64_t& aParentID) override;

    void LoadURL(nsIURI* aURI);
    // XXX/cjones: it's not clear what we gain by hiding these
    // message-sending functions under a layer of indirection and
    // eating the return values
    void Show(const ScreenIntSize& size, bool aParentIsActive);
    void UpdateDimensions(const nsIntRect& rect, const ScreenIntSize& size);
    void UpdateFrame(const layers::FrameMetrics& aFrameMetrics);
    void UIResolutionChanged();
    void ThemeChanged();
    void RequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                          const mozilla::CSSPoint& aDestination);
    void AcknowledgeScrollUpdate(const ViewID& aScrollId, const uint32_t& aScrollGeneration);
    void HandleDoubleTap(const CSSPoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid);
    void HandleSingleTap(const CSSPoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid);
    void HandleLongTap(const CSSPoint& aPoint,
                       Modifiers aModifiers,
                       const ScrollableLayerGuid& aGuid,
                       uint64_t aInputBlockId);
    void NotifyAPZStateChange(ViewID aViewId,
                              APZStateChange aChange,
                              int aArg);
    void NotifyMouseScrollTestEvent(const ViewID& aScrollId, const nsString& aEvent);
    void NotifyFlushComplete();
    void Activate();
    void Deactivate();

    bool MapEventCoordinatesForChildProcess(mozilla::WidgetEvent* aEvent);
    void MapEventCoordinatesForChildProcess(const LayoutDeviceIntPoint& aOffset,
                                            mozilla::WidgetEvent* aEvent);
    LayoutDeviceToCSSScale GetLayoutDeviceToCSSScale();

    virtual bool RecvRequestNativeKeyBindings(const mozilla::WidgetKeyboardEvent& aEvent,
                                              MaybeNativeKeyBinding* aBindings) override;

    virtual bool RecvSynthesizeNativeKeyEvent(const int32_t& aNativeKeyboardLayout,
                                              const int32_t& aNativeKeyCode,
                                              const uint32_t& aModifierFlags,
                                              const nsString& aCharacters,
                                              const nsString& aUnmodifiedCharacters,
                                              const uint64_t& aObserverId) override;
    virtual bool RecvSynthesizeNativeMouseEvent(const LayoutDeviceIntPoint& aPoint,
                                                const uint32_t& aNativeMessage,
                                                const uint32_t& aModifierFlags,
                                                const uint64_t& aObserverId) override;
    virtual bool RecvSynthesizeNativeMouseMove(const LayoutDeviceIntPoint& aPoint,
                                               const uint64_t& aObserverId) override;
    virtual bool RecvSynthesizeNativeMouseScrollEvent(const LayoutDeviceIntPoint& aPoint,
                                                      const uint32_t& aNativeMessage,
                                                      const double& aDeltaX,
                                                      const double& aDeltaY,
                                                      const double& aDeltaZ,
                                                      const uint32_t& aModifierFlags,
                                                      const uint32_t& aAdditionalFlags,
                                                      const uint64_t& aObserverId) override;
    virtual bool RecvSynthesizeNativeTouchPoint(const uint32_t& aPointerId,
                                                const TouchPointerState& aPointerState,
                                                const nsIntPoint& aPointerScreenPoint,
                                                const double& aPointerPressure,
                                                const uint32_t& aPointerOrientation,
                                                const uint64_t& aObserverId) override;
    virtual bool RecvSynthesizeNativeTouchTap(const nsIntPoint& aPointerScreenPoint,
                                              const bool& aLongTap,
                                              const uint64_t& aObserverId) override;
    virtual bool RecvClearNativeTouchSequence(const uint64_t& aObserverId) override;

    void SendMouseEvent(const nsAString& aType, float aX, float aY,
                        int32_t aButton, int32_t aClickCount,
                        int32_t aModifiers, bool aIgnoreRootScrollFrame);
    void SendKeyEvent(const nsAString& aType, int32_t aKeyCode,
                      int32_t aCharCode, int32_t aModifiers,
                      bool aPreventDefault);
    bool SendRealMouseEvent(mozilla::WidgetMouseEvent& event);
    bool SendRealDragEvent(mozilla::WidgetDragEvent& aEvent, uint32_t aDragAction,
                           uint32_t aDropEffect);
    bool SendMouseWheelEvent(mozilla::WidgetWheelEvent& event);
    bool SendRealKeyEvent(mozilla::WidgetKeyboardEvent& event);
    bool SendRealTouchEvent(WidgetTouchEvent& event);
    bool SendHandleSingleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid);
    bool SendHandleLongTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId);
    bool SendHandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid);

    virtual PDocumentRendererParent*
    AllocPDocumentRendererParent(const nsRect& documentRect,
                                 const gfx::Matrix& transform,
                                 const nsString& bgcolor,
                                 const uint32_t& renderFlags,
                                 const bool& flushLayout,
                                 const nsIntSize& renderSize) override;
    virtual bool DeallocPDocumentRendererParent(PDocumentRendererParent* actor) override;

    virtual PFilePickerParent*
    AllocPFilePickerParent(const nsString& aTitle,
                           const int16_t& aMode) override;
    virtual bool DeallocPFilePickerParent(PFilePickerParent* actor) override;

    virtual PIndexedDBPermissionRequestParent*
    AllocPIndexedDBPermissionRequestParent(const Principal& aPrincipal)
                                           override;

    virtual bool
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
    bool SendCompositionEvent(mozilla::WidgetCompositionEvent& event);
    bool SendSelectionEvent(mozilla::WidgetSelectionEvent& event);

    static TabParent* GetFrom(nsFrameLoader* aFrameLoader);
    static TabParent* GetFrom(nsIFrameLoader* aFrameLoader);
    static TabParent* GetFrom(nsITabParent* aTabParent);
    static TabParent* GetFrom(PBrowserParent* aTabParent);
    static TabParent* GetFrom(nsIContent* aContent);
    static TabId GetTabIdFrom(nsIDocShell* docshell);

    nsIContentParent* Manager() { return mManager; }

    /**
     * Let managees query if Destroy() is already called so they don't send out
     * messages when the PBrowser actor is being destroyed.
     */
    bool IsDestroyed() const { return mIsDestroyed; }

    already_AddRefed<nsIWidget> GetWidget() const;

    const TabId GetTabId() const
    {
      return mTabId;
    }

    LayoutDeviceIntPoint GetChildProcessOffset();

    /**
     * Native widget remoting protocol for use with windowed plugins with e10s.
     */
    virtual PPluginWidgetParent* AllocPPluginWidgetParent() override;
    virtual bool DeallocPPluginWidgetParent(PPluginWidgetParent* aActor) override;

    void SetInitedByParent() { mInitedByParent = true; }
    bool IsInitedByParent() const { return mInitedByParent; }

    static TabParent* GetNextTabParent();

    bool SendLoadRemoteScript(const nsString& aURL,
                              const bool& aRunInGlobalScope);

    // See nsIFrameLoader requestNotifyLayerTreeReady.
    bool RequestNotifyLayerTreeReady();
    bool RequestNotifyLayerTreeCleared();
    bool LayerTreeUpdate(bool aActive);
    void SwapLayerTreeObservers(TabParent* aOther);

    virtual bool
    RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                          const uint32_t& aAction,
                          const nsCString& aVisualDnDData,
                          const uint32_t& aWidth, const uint32_t& aHeight,
                          const uint32_t& aStride, const uint8_t& aFormat,
                          const int32_t& aDragAreaX, const int32_t& aDragAreaY) override;

    void AddInitialDnDDataTo(DataTransfer* aDataTransfer);

    void TakeDragVisualization(RefPtr<mozilla::gfx::SourceSurface>& aSurface,
                               int32_t& aDragAreaX, int32_t& aDragAreaY);
    layout::RenderFrameParent* GetRenderFrame();

    virtual PWebBrowserPersistDocumentParent* AllocPWebBrowserPersistDocumentParent(const uint64_t& aOuterWindowID) override;
    virtual bool DeallocPWebBrowserPersistDocumentParent(PWebBrowserPersistDocumentParent* aActor) override;

protected:
    bool ReceiveMessage(const nsString& aMessage,
                        bool aSync,
                        const StructuredCloneData* aCloneData,
                        mozilla::jsipc::CpowHolder* aCpows,
                        nsIPrincipal* aPrincipal,
                        nsTArray<OwningSerializedStructuredCloneBuffer>* aJSONRetVal = nullptr);

    virtual bool RecvAsyncAuthPrompt(const nsCString& aUri,
                                     const nsString& aRealm,
                                     const uint64_t& aCallbackId) override;

    virtual bool Recv__delete__() override;

    virtual void ActorDestroy(ActorDestroyReason why) override;

    Element* mFrameElement;
    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;

    virtual PRenderFrameParent* AllocPRenderFrameParent() override;
    virtual bool DeallocPRenderFrameParent(PRenderFrameParent* aFrame) override;

    virtual bool RecvRemotePaintIsReady() override;

    virtual bool RecvGetRenderFrameInfo(PRenderFrameParent* aRenderFrame,
                                        TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                        uint64_t* aLayersId) override;

    virtual bool RecvSetDimensions(const uint32_t& aFlags,
                                   const int32_t& aX, const int32_t& aY,
                                   const int32_t& aCx, const int32_t& aCy) override;

    virtual bool RecvAudioChannelActivityNotification(const uint32_t& aAudioChannel,
                                                      const bool& aActive) override;

    bool InitBrowserConfiguration(const nsCString& aURI,
                                  BrowserConfiguration& aConfiguration);

    void SetHasContentOpener(bool aHasContentOpener);

    ContentCacheInParent mContentCache;

    nsIntRect mRect;
    ScreenIntSize mDimensions;
    ScreenOrientationInternal mOrientation;
    float mDPI;
    CSSToLayoutDeviceScale mDefaultScale;
    bool mUpdatedDimensions;
    LayoutDeviceIntPoint mChromeOffset;

private:
    already_AddRefed<nsFrameLoader> GetFrameLoader(bool aUseCachedFrameLoaderAfterDestroy = false) const;
    nsRefPtr<nsIContentParent> mManager;
    void TryCacheDPIAndScale();

    nsresult UpdatePosition();

    CSSPoint AdjustTapToChildWidget(const CSSPoint& aPoint);

    bool AsyncPanZoomEnabled() const;

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
    // Whether we have already sent a FileDescriptor for the app package.
    bool mAppPackageFileDescriptorSent;

    // Whether we need to send the offline status to the TabChild
    // This is true, until the first call of LoadURL
    bool mSendOfflineStatus;

    uint32_t mChromeFlags;

    struct DataTransferItem
    {
      nsCString mFlavor;
      nsString mStringData;
      nsRefPtr<mozilla::dom::BlobImpl> mBlobData;
      enum DataType
      {
        eString,
        eBlob
      };
      DataType mType;
    };
    nsTArray<nsTArray<DataTransferItem>> mInitialDataTransferItems;

    mozilla::RefPtr<gfx::DataSourceSurface> mDnDVisualization;
    int32_t mDragAreaX;
    int32_t mDragAreaY;

    // When true, the TabParent is initialized without child side's request.
    // When false, the TabParent is initialized by window.open() from child side.
    bool mInitedByParent;

    nsCOMPtr<nsILoadContext> mLoadContext;

    // We keep a strong reference to the frameloader after we've sent the
    // Destroy message and before we've received __delete__. This allows us to
    // dispatch message manager messages during this time.
    nsRefPtr<nsFrameLoader> mFrameLoader;

    TabId mTabId;

    // Helper class for RecvCreateWindow.
    struct AutoUseNewTab;

    // When loading a new tab or window via window.open, the child process sends
    // a new PBrowser to use. We store that tab in sNextTabParent and then
    // proceed through the browser's normal paths to create a new
    // window/tab. When it comes time to create a new TabParent, we instead use
    // sNextTabParent.
    static TabParent* sNextTabParent;

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

    // If the user called RequestNotifyLayerTreeReady and the RenderFrameParent
    // wasn't ready yet, we set this flag and call RequestNotifyLayerTreeReady
    // again once the RenderFrameParent arrives.
    bool mNeedLayerTreeReadyNotification;

    // Cached cursor setting from TabChild.  When the cursor is over the tab,
    // it should take this appearance.
    nsCursor mCursor;
    nsCOMPtr<imgIContainer> mCustomCursor;
    uint32_t mCustomCursorHotspotX, mCustomCursorHotspotY;

    // True if the cursor changes from the TabChild should change the widget
    // cursor.  This happens whenever the cursor is in the tab's region.
    bool mTabSetsCursor;

    nsRefPtr<nsIPresShell> mPresShellWithRefreshListener;

    bool mHasContentOpener;

    DebugOnly<int32_t> mActiveSupressDisplayportCount;
private:
    // This is used when APZ needs to find the TabParent associated with a layer
    // to dispatch events.
    typedef nsDataHashtable<nsUint64HashKey, TabParent*> LayerToTabParentTable;
    static LayerToTabParentTable* sLayerToTabParentTable;

    static void AddTabParentToTable(uint64_t aLayersId, TabParent* aTabParent);
    static void RemoveTabParentFromTable(uint64_t aLayersId);

public:
    static TabParent* GetTabParentFromLayersId(uint64_t aLayersId);
};

} // namespace dom
} // namespace mozilla

#endif

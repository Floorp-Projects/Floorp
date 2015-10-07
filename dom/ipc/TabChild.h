/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabChild_h
#define mozilla_dom_TabChild_h

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
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "nsIWebBrowserChrome3.h"
#include "mozilla/dom/ipc/IdType.h"
#include "AudioChannelService.h"
#include "PuppetWidget.h"

class nsICachedFileDescriptorListener;
class nsIDOMWindowUtils;

namespace mozilla {
namespace layout {
class RenderFrameChild;
} // namespace layout

namespace layers {
class APZEventState;
class ImageCompositeNotification;
} // namespace layers

namespace widget {
struct AutoCacheNativeKeyCommands;
} // namespace widget

namespace plugins {
class PluginWidgetChild;
} // namespace plugins

namespace dom {

class TabChild;
class ClonedMessageData;
class TabChildBase;

class TabChildGlobal : public DOMEventTargetHelper,
                       public nsIContentFrameMessageManager,
                       public nsIScriptObjectPrincipal,
                       public nsIGlobalObject,
                       public nsSupportsWeakReference
{
public:
  explicit TabChildGlobal(TabChildBase* aTabChild);
  void Init();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TabChildGlobal, DOMEventTargetHelper)
  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGEMANAGERGLOBAL(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObject,
                             JS::Handle<JS::Value> aRemote,
                             nsIPrincipal* aPrincipal,
                             JSContext* aCx,
                             uint8_t aArgc,
                             JS::MutableHandle<JS::Value> aRetval) override
  {
    return mMessageManager
      ? mMessageManager->SendSyncMessage(aMessageName, aObject, aRemote,
                                         aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD SendRpcMessage(const nsAString& aMessageName,
                            JS::Handle<JS::Value> aObject,
                            JS::Handle<JS::Value> aRemote,
                            nsIPrincipal* aPrincipal,
                            JSContext* aCx,
                            uint8_t aArgc,
                            JS::MutableHandle<JS::Value> aRetval) override
  {
    return mMessageManager
      ? mMessageManager->SendRpcMessage(aMessageName, aObject, aRemote,
                                        aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(nsIDOMWindow** aContent) override;
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell) override;

  nsresult AddEventListener(const nsAString& aType,
                            nsIDOMEventListener* aListener,
                            bool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                  aUseCapture, false, 2);
  }
  using DOMEventTargetHelper::AddEventListener;
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture, bool aWantsUntrusted,
                              uint8_t optional_argc) override
  {
    return DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                  aUseCapture,
                                                  aWantsUntrusted,
                                                  optional_argc);
  }

  nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) override
  {
    aVisitor.mForceContentDispatch = true;
    return NS_OK;
  }

  virtual JSContext* GetJSContextForEventHandlers() override;
  virtual nsIPrincipal* GetPrincipal() override;
  virtual JSObject* GetGlobalJSObject() override;

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("TabChildGlobal doesn't use DOM bindings!");
  }

  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;
  nsRefPtr<TabChildBase> mTabChild;

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

// This is base clase which helps to share Viewport and touch related functionality
// between b2g/android FF/embedlite clients implementation.
// It make sense to place in this class all helper functions, and functionality which could be shared between
// Cross-process/Cross-thread implmentations.
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

    virtual nsIWebNavigation* WebNavigation() const = 0;
    virtual PuppetWidget* WebWidget() = 0;
    nsIPrincipal* GetPrincipal() { return mPrincipal; }
    virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                         const mozilla::layers::FrameMetrics::ViewID& aViewId,
                                         const Maybe<mozilla::layers::ZoomConstraints>& aConstraints) = 0;

    virtual ScreenIntSize GetInnerSize() = 0;

    // Get the Document for the top-level window in this tab.
    already_AddRefed<nsIDocument> GetDocument() const;

protected:
    virtual ~TabChildBase();

    // Get the pres-shell of the document for the top-level window in this tab.
    already_AddRefed<nsIPresShell> GetPresShell() const;

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
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
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
                       public nsITooltipListener
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::layout::RenderFrameChild RenderFrameChild;
    typedef mozilla::layers::APZEventState APZEventState;
    typedef mozilla::layers::SetAllowedTouchBehaviorCallback SetAllowedTouchBehaviorCallback;

public:
    /**
     * Find TabChild of aTabId in the same content process of the
     * caller.
     */
    static already_AddRefed<TabChild> FindTabChild(const TabId& aTabId);

public:
    /**
     * This is expected to be called off the critical path to content
     * startup.  This is an opportunity to load things that are slow
     * on the critical path.
     */
    static void PreloadSlowThings();

    /** Return a TabChild with the given attributes. */
    static already_AddRefed<TabChild>
    Create(nsIContentChild* aManager, const TabId& aTabId, const TabContext& aContext, uint32_t aChromeFlags);

    bool IsRootContentDocument();
    // Let managees query if it is safe to send messages.
    bool IsDestroyed() { return mDestroyed; }
    const TabId GetTabId() const {
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
    virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                         const ViewID& aViewId,
                                         const Maybe<ZoomConstraints>& aConstraints) override;
    virtual bool RecvLoadURL(const nsCString& aURI,
                             const BrowserConfiguration& aConfiguration) override;
    virtual bool RecvCacheFileDescriptor(const nsString& aPath,
                                         const FileDescriptor& aFileDescriptor)
                                         override;
    virtual bool RecvShow(const ScreenIntSize& aSize,
                          const ShowInfo& aInfo,
                          const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                          const uint64_t& aLayersId,
                          PRenderFrameChild* aRenderFrame,
                          const bool& aParentIsActive) override;
    virtual bool RecvUpdateDimensions(const CSSRect& rect,
                                      const CSSSize& size,
                                      const nsSizeMode& sizeMode,
                                      const ScreenOrientationInternal& orientation,
                                      const LayoutDeviceIntPoint& chromeDisp) override;
    virtual bool RecvUpdateFrame(const layers::FrameMetrics& aFrameMetrics) override;
    virtual bool RecvRequestFlingSnap(const ViewID& aScrollId,
                                      const CSSPoint& aDestination) override;
    virtual bool RecvAcknowledgeScrollUpdate(const ViewID& aScrollId,
                                             const uint32_t& aScrollGeneration) override;
    virtual bool RecvHandleDoubleTap(const CSSPoint& aPoint,
                                     const Modifiers& aModifiers,
                                     const mozilla::layers::ScrollableLayerGuid& aGuid) override;
    virtual bool RecvHandleSingleTap(const CSSPoint& aPoint,
                                     const Modifiers& aModifiers,
                                     const mozilla::layers::ScrollableLayerGuid& aGuid) override;
    virtual bool RecvHandleLongTap(const CSSPoint& aPoint,
                                   const Modifiers& aModifiers,
                                   const mozilla::layers::ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId) override;
    virtual bool RecvNotifyAPZStateChange(const ViewID& aViewId,
                                          const APZStateChange& aChange,
                                          const int& aArg) override;
    virtual bool RecvNotifyFlushComplete() override;
    virtual bool RecvActivate() override;
    virtual bool RecvDeactivate() override;
    virtual bool RecvMouseEvent(const nsString& aType,
                                const float&    aX,
                                const float&    aY,
                                const int32_t&  aButton,
                                const int32_t&  aClickCount,
                                const int32_t&  aModifiers,
                                const bool&     aIgnoreRootScrollFrame) override;
    virtual bool RecvRealMouseMoveEvent(const mozilla::WidgetMouseEvent& event,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId) override;
    virtual bool RecvSynthMouseMoveEvent(const mozilla::WidgetMouseEvent& event,
                                         const ScrollableLayerGuid& aGuid,
                                         const uint64_t& aInputBlockId) override;
    virtual bool RecvRealMouseButtonEvent(const mozilla::WidgetMouseEvent& event,
                                          const ScrollableLayerGuid& aGuid,
                                          const uint64_t& aInputBlockId) override;
    virtual bool RecvRealDragEvent(const WidgetDragEvent& aEvent,
                                   const uint32_t& aDragAction,
                                   const uint32_t& aDropEffect) override;
    virtual bool RecvRealKeyEvent(const mozilla::WidgetKeyboardEvent& event,
                                  const MaybeNativeKeyBinding& aBindings) override;
    virtual bool RecvMouseWheelEvent(const mozilla::WidgetWheelEvent& event,
                                     const ScrollableLayerGuid& aGuid,
                                     const uint64_t& aInputBlockId) override;
    virtual bool RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                                    const ScrollableLayerGuid& aGuid,
                                    const uint64_t& aInputBlockId,
                                    const nsEventStatus& aApzResponse) override;
    virtual bool RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId,
                                        const nsEventStatus& aApzResponse) override;
    virtual bool RecvKeyEvent(const nsString& aType,
                              const int32_t&  aKeyCode,
                              const int32_t&  aCharCode,
                              const int32_t&  aModifiers,
                              const bool&     aPreventDefault) override;
    virtual bool RecvMouseScrollTestEvent(const FrameMetrics::ViewID& aScrollId,
                                          const nsString& aEvent) override;
    virtual bool RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                             const nsCString& aResponse) override;
    virtual bool RecvCompositionEvent(const mozilla::WidgetCompositionEvent& event) override;
    virtual bool RecvSelectionEvent(const mozilla::WidgetSelectionEvent& event) override;
    virtual bool RecvActivateFrameEvent(const nsString& aType, const bool& capture) override;
    virtual bool RecvLoadRemoteScript(const nsString& aURL,
                                      const bool& aRunInGlobalScope) override;
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData,
                                  InfallibleTArray<CpowEntry>&& aCpows,
                                  const IPC::Principal& aPrincipal) override;

    virtual bool RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline) override;

    virtual bool RecvSwappedWithOtherRemoteLoader() override;

    virtual PDocAccessibleChild* AllocPDocAccessibleChild(PDocAccessibleChild*,
                                                          const uint64_t&)
      override;
    virtual bool DeallocPDocAccessibleChild(PDocAccessibleChild*) override;
    virtual PDocumentRendererChild*
    AllocPDocumentRendererChild(const nsRect& documentRect, const gfx::Matrix& transform,
                                const nsString& bgcolor,
                                const uint32_t& renderFlags, const bool& flushLayout,
                                const nsIntSize& renderSize) override;
    virtual bool DeallocPDocumentRendererChild(PDocumentRendererChild* actor) override;
    virtual bool RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                                  const nsRect& documentRect,
                                                  const gfx::Matrix& transform,
                                                  const nsString& bgcolor,
                                                  const uint32_t& renderFlags,
                                                  const bool& flushLayout,
                                                  const nsIntSize& renderSize) override;

    virtual PColorPickerChild*
    AllocPColorPickerChild(const nsString& title, const nsString& initialColor) override;
    virtual bool DeallocPColorPickerChild(PColorPickerChild* actor) override;

    virtual PFilePickerChild*
    AllocPFilePickerChild(const nsString& aTitle, const int16_t& aMode) override;
    virtual bool
    DeallocPFilePickerChild(PFilePickerChild* actor) override;

    virtual PIndexedDBPermissionRequestChild*
    AllocPIndexedDBPermissionRequestChild(const Principal& aPrincipal)
                                          override;

    virtual bool
    DeallocPIndexedDBPermissionRequestChild(
                                       PIndexedDBPermissionRequestChild* aActor)
                                       override;

    virtual nsIWebNavigation* WebNavigation() const override { return mWebNav; }
    virtual PuppetWidget* WebWidget() override { return mPuppetWidget; }

    /** Return the DPI of the widget this TabChild draws to. */
    void GetDPI(float* aDPI);
    void GetDefaultScale(double *aScale);

    void GetMaxTouchPoints(uint32_t* aTouchPoints);

    ScreenOrientationInternal GetOrientation() const { return mOrientation; }

    void SetBackgroundColor(const nscolor& aColor);

    void NotifyPainted();

    void RequestNativeKeyBindings(mozilla::widget::AutoCacheNativeKeyCommands* aAutoCache,
                                  WidgetKeyboardEvent* aEvent);

    /**
     * Signal to this TabChild that it should be made visible:
     * activated widget, retained layer tree, etc.  (Respectively,
     * made not visible.)
     */
    void MakeVisible();
    void MakeHidden();

    // Returns true if the file descriptor was found in the cache, false
    // otherwise.
    bool GetCachedFileDescriptor(const nsAString& aPath,
                                 nsICachedFileDescriptorListener* aCallback);

    void CancelCachedFileDescriptorCallback(
                                    const nsAString& aPath,
                                    nsICachedFileDescriptorListener* aCallback);

    nsIContentChild* Manager() { return mManager; }

    bool GetUpdateHitRegion() { return mUpdateHitRegion; }

    void UpdateHitRegion(const nsRegion& aRegion);

    static inline TabChild*
    GetFrom(nsIDocShell* aDocShell)
    {
      nsCOMPtr<nsITabChild> tc = do_GetInterface(aDocShell);
      return static_cast<TabChild*>(tc.get());
    }

    static TabChild* GetFrom(nsIPresShell* aPresShell);
    static TabChild* GetFrom(uint64_t aLayersId);

    void DidComposite(uint64_t aTransactionId,
                      const TimeStamp& aCompositeStart,
                      const TimeStamp& aCompositeEnd);
    void DidRequestComposite(const TimeStamp& aCompositeReqStart,
                             const TimeStamp& aCompositeReqEnd);

    void ClearCachedResources();

    static inline TabChild*
    GetFrom(nsIDOMWindow* aWindow)
    {
      nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
      return GetFrom(docShell);
    }

    virtual bool RecvUIResolutionChanged(const float& aDpi, const double& aScale) override;

    virtual bool RecvThemeChanged(nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache) override;

    virtual bool RecvHandleAccessKey(nsTArray<uint32_t>&& aCharCodes,
                                     const bool& aIsTrusted,
                                     const int32_t& aModifierMask) override;

    /**
     * Native widget remoting protocol for use with windowed plugins with e10s.
     */
    PPluginWidgetChild* AllocPPluginWidgetChild() override;
    bool DeallocPPluginWidgetChild(PPluginWidgetChild* aActor) override;
    nsresult CreatePluginWidget(nsIWidget* aParent, nsIWidget** aOut);

    LayoutDeviceIntPoint GetChromeDisplacement() { return mChromeDisp; };

    bool IPCOpen() { return mIPCOpen; }

    bool ParentIsActive()
    {
      return mParentIsActive;
    }
    bool AsyncPanZoomEnabled() { return mAsyncPanZoomEnabled; }

    virtual ScreenIntSize GetInnerSize() override;

protected:
    virtual ~TabChild();

    virtual PRenderFrameChild* AllocPRenderFrameChild() override;
    virtual bool DeallocPRenderFrameChild(PRenderFrameChild* aFrame) override;
    virtual bool RecvDestroy() override;
    virtual bool RecvSetUpdateHitRegion(const bool& aEnabled) override;
    virtual bool RecvSetDocShellIsActive(const bool& aIsActive) override;
    virtual bool RecvNavigateByKey(const bool& aForward, const bool& aForDocumentNavigation) override;

    virtual bool RecvRequestNotifyAfterRemotePaint() override;

    virtual bool RecvSuppressDisplayport(const bool& aEnabled) override;

    virtual bool RecvParentActivated(const bool& aActivated) override;

    virtual bool RecvStopIMEStateManagement() override;
    virtual bool RecvMenuKeyboardListenerInstalled(
                   const bool& aInstalled) override;

#ifdef MOZ_WIDGET_GONK
    void MaybeRequestPreinitCamera();
#endif

private:
    /**
     * Create a new TabChild object.
     */
    TabChild(nsIContentChild* aManager,
             const TabId& aTabId,
             const TabContext& aContext,
             uint32_t aChromeFlags);

    nsresult Init();

    class DelayedFireContextMenuEvent;

    // Notify others that our TabContext has been updated.  (At the moment, this
    // sets the appropriate app-id and is-browser flags on our docshell.)
    //
    // You should call this after calling TabContext::SetTabContext().  We also
    // call this during Init().
    void NotifyTabContextUpdated();

    void ActorDestroy(ActorDestroyReason why) override;

    enum FrameScriptLoading { DONT_LOAD_SCRIPTS, DEFAULT_LOAD_SCRIPTS };
    bool InitTabChildGlobal(FrameScriptLoading aScriptLoading = DEFAULT_LOAD_SCRIPTS);
    bool InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                            const uint64_t& aLayersId,
                            PRenderFrameChild* aRenderFrame);
    void DestroyWindow();
    void SetProcessNameToAppName();

    // Call RecvShow(nsIntSize(0, 0)) and block future calls to RecvShow().
    void DoFakeShow(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                    const uint64_t& aLayersId,
                    PRenderFrameChild* aRenderFrame);

    void ApplyShowInfo(const ShowInfo& aInfo);

    // These methods are used for tracking synthetic mouse events
    // dispatched for compatibility.  On each touch event, we
    // UpdateTapState().  If we've detected that the current gesture
    // isn't a tap, then we CancelTapTracking().  In the meantime, we
    // may detect a context-menu event, and if so we
    // FireContextMenuEvent().
    void FireContextMenuEvent();
    void CancelTapTracking();
    void UpdateTapState(const WidgetTouchEvent& aEvent, nsEventStatus aStatus);

    nsresult
    ProvideWindowCommon(nsIDOMWindow* aOpener,
                        bool aIframeMoz,
                        uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified,
                        bool aSizeSpecified,
                        nsIURI* aURI,
                        const nsAString& aName,
                        const nsACString& aFeatures,
                        bool* aWindowIsNew,
                        nsIDOMWindow** aReturn);

    bool HasValidInnerSize();

    void SetTabId(const TabId& aTabId);

    ScreenIntRect GetOuterRect();

    void SetUnscaledInnerSize(const CSSSize& aSize) {
      mUnscaledInnerSize = aSize;
    }

    class CachedFileDescriptorInfo;
    class CachedFileDescriptorCallbackRunnable;
    class DelayedDeleteRunnable;

    TextureFactoryIdentifier mTextureFactoryIdentifier;
    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsRefPtr<PuppetWidget> mPuppetWidget;
    nsCOMPtr<nsIURI> mLastURI;
    RenderFrameChild* mRemoteFrame;
    nsRefPtr<nsIContentChild> mManager;
    uint32_t mChromeFlags;
    int32_t mActiveSuppressDisplayport;
    uint64_t mLayersId;
    CSSRect mUnscaledOuterRect;
    // When we're tracking a possible tap gesture, this is the "down"
    // point of the touchstart.
    LayoutDevicePoint mGestureDownPoint;
    // The touch identifier of the active gesture.
    int32_t mActivePointerId;
    // A timer task that fires if the tap-hold timeout is exceeded by
    // the touch we're tracking.  That is, if touchend or a touchmove
    // that exceeds the gesture threshold doesn't happen.
    nsCOMPtr<nsITimer> mTapHoldTimer;
    // Whether we have already received a FileDescriptor for the app package.
    bool mAppPackageFileDescriptorRecved;
    // At present only 1 of these is really expected.
    nsAutoTArray<nsAutoPtr<CachedFileDescriptorInfo>, 1>
        mCachedFileDescriptorInfos;
    nscolor mLastBackgroundColor;
    bool mDidFakeShow;
    bool mNotified;
    bool mTriedBrowserInit;
    ScreenOrientationInternal mOrientation;
    bool mUpdateHitRegion;

    bool mIgnoreKeyPressEvent;
    nsRefPtr<APZEventState> mAPZEventState;
    SetAllowedTouchBehaviorCallback mSetAllowedTouchBehaviorCallback;
    bool mHasValidInnerSize;
    bool mDestroyed;
    // Position of tab, relative to parent widget (typically the window)
    LayoutDeviceIntPoint mChromeDisp;
    TabId mUniqueId;
    float mDPI;
    double mDefaultScale;
    bool mIPCOpen;
    bool mParentIsActive;
    bool mAsyncPanZoomEnabled;
    CSSSize mUnscaledInnerSize;

    nsAutoTArray<bool, NUMBER_OF_AUDIO_CHANNELS> mAudioChannelsActive;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TabChild_h

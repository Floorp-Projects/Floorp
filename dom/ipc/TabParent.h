/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "mozilla/EventForwards.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/PContentDialogParent.h"
#include "mozilla/dom/TabContext.h"
#include "nsCOMPtr.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsISecureBrowserUI.h"
#include "nsITabParent.h"
#include "Units.h"
#include "js/TypeDecls.h"

struct gfxMatrix;
class nsFrameLoader;
class nsIContent;
class nsIPrincipal;
class nsIURI;
class nsIWidget;
class CpowHolder;

namespace mozilla {

namespace layers {
struct FrameMetrics;
struct TextureFactoryIdentifier;
}

namespace layout {
class RenderFrameParent;
}

namespace dom {

class ClonedMessageData;
class ContentParent;
class Element;
struct StructuredCloneData;

class ContentDialogParent : public PContentDialogParent {};

class TabParent : public PBrowserParent 
                , public nsITabParent 
                , public nsIAuthPromptProvider
                , public nsISecureBrowserUI
                , public TabContext
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::layout::ScrollingBehavior ScrollingBehavior;

public:
    TabParent(ContentParent* aManager, const TabContext& aContext);
    virtual ~TabParent();
    Element* GetOwnerElement() const { return mFrameElement; }
    void SetOwnerElement(Element* aElement);

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

    /**
     * Return the TabParent that has decided it wants to capture an
     * event series for fast-path dispatch to its subprocess, if one
     * has.
     *
     * DOM event dispatch and widget are free to ignore capture
     * requests from TabParents; the end result wrt remote content is
     * (must be) always the same, albeit usually slower without
     * subprocess capturing.  This allows frontends/widget backends to
     * "opt in" to faster cross-process dispatch.
     */
    static TabParent* GetEventCapturer();
    /**
     * If this is the current event capturer, give this a chance to
     * capture the event.  If it was captured, return true, false
     * otherwise.  Un-captured events should follow normal DOM
     * dispatch; captured events should result in no further
     * processing from the caller of TryCapture().
     *
     * It's an error to call TryCapture() if this isn't the event
     * capturer.
     */
    bool TryCapture(const WidgetGUIEvent& aEvent);

    void Destroy();

    virtual bool RecvMoveFocus(const bool& aForward);
    virtual bool RecvEvent(const RemoteDOMEvent& aEvent);
    virtual bool RecvPRenderFrameConstructor(PRenderFrameParent* actor,
                                             ScrollingBehavior* scrolling,
                                             TextureFactoryIdentifier* identifier,
                                             uint64_t* layersId);
    virtual bool RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                            const nsString& aURL,
                                            const nsString& aName,
                                            const nsString& aFeatures,
                                            bool* aOutWindowOpened);
    virtual bool AnswerCreateWindow(PBrowserParent** retval);
    virtual bool RecvSyncMessage(const nsString& aMessage,
                                 const ClonedMessageData& aData,
                                 const InfallibleTArray<CpowEntry>& aCpows,
                                 const IPC::Principal& aPrincipal,
                                 InfallibleTArray<nsString>* aJSONRetVal);
    virtual bool AnswerRpcMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal,
                                  InfallibleTArray<nsString>* aJSONRetVal);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData,
                                  const InfallibleTArray<CpowEntry>& aCpows,
                                  const IPC::Principal& aPrincipal);
    virtual bool RecvNotifyIMEFocus(const bool& aFocus,
                                    nsIMEUpdatePreference* aPreference,
                                    uint32_t* aSeqno);
    virtual bool RecvNotifyIMETextChange(const uint32_t& aStart,
                                         const uint32_t& aEnd,
                                         const uint32_t& aNewEnd);
    virtual bool RecvNotifyIMESelection(const uint32_t& aSeqno,
                                        const uint32_t& aAnchor,
                                        const uint32_t& aFocus);
    virtual bool RecvNotifyIMETextHint(const nsString& aText);
    virtual bool RecvEndIMEComposition(const bool& aCancel,
                                       nsString* aComposition);
    virtual bool RecvGetInputContext(int32_t* aIMEEnabled,
                                     int32_t* aIMEOpen,
                                     intptr_t* aNativeIMEContext);
    virtual bool RecvSetInputContext(const int32_t& aIMEEnabled,
                                     const int32_t& aIMEOpen,
                                     const nsString& aType,
                                     const nsString& aInputmode,
                                     const nsString& aActionHint,
                                     const int32_t& aCause,
                                     const int32_t& aFocusChange);
    virtual bool RecvRequestFocus(const bool& aCanRaise);
    virtual bool RecvSetCursor(const uint32_t& aValue);
    virtual bool RecvSetBackgroundColor(const nscolor& aValue);
    virtual bool RecvSetStatus(const uint32_t& aType, const nsString& aStatus);
    virtual bool RecvGetDPI(float* aValue);
    virtual bool RecvGetDefaultScale(double* aValue);
    virtual bool RecvGetWidgetNativeData(WindowsHandle* aValue);
    virtual bool RecvZoomToRect(const uint32_t& aPresShellId,
                                const ViewID& aViewId,
                                const CSSRect& aRect);
    virtual bool RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                                           const ViewID& aViewId,
                                           const bool& aIsRoot,
                                           const bool& aAllowZoom,
                                           const CSSToScreenScale& aMinZoom,
                                           const CSSToScreenScale& aMaxZoom);
    virtual bool RecvUpdateScrollOffset(const uint32_t& aPresShellId, const ViewID& aViewId, const CSSIntPoint& aScrollOffset);
    virtual bool RecvContentReceivedTouch(const ScrollableLayerGuid& aGuid,
                                          const bool& aPreventDefault);
    virtual PContentDialogParent* AllocPContentDialogParent(const uint32_t& aType,
                                                            const nsCString& aName,
                                                            const nsCString& aFeatures,
                                                            const InfallibleTArray<int>& aIntParams,
                                                            const InfallibleTArray<nsString>& aStringParams);
    virtual bool DeallocPContentDialogParent(PContentDialogParent* aDialog)
    {
      delete aDialog;
      return true;
    }


    void LoadURL(nsIURI* aURI);
    // XXX/cjones: it's not clear what we gain by hiding these
    // message-sending functions under a layer of indirection and
    // eating the return values
    void Show(const nsIntSize& size);
    void UpdateDimensions(const nsRect& rect, const nsIntSize& size);
    void UpdateFrame(const layers::FrameMetrics& aFrameMetrics);
    void HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers);
    void HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers);
    void HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers);
    void Activate();
    void Deactivate();

    bool MapEventCoordinatesForChildProcess(mozilla::WidgetEvent* aEvent);
    void MapEventCoordinatesForChildProcess(const LayoutDeviceIntPoint& aOffset,
                                            mozilla::WidgetEvent* aEvent);

    void SendMouseEvent(const nsAString& aType, float aX, float aY,
                        int32_t aButton, int32_t aClickCount,
                        int32_t aModifiers, bool aIgnoreRootScrollFrame);
    void SendKeyEvent(const nsAString& aType, int32_t aKeyCode,
                      int32_t aCharCode, int32_t aModifiers,
                      bool aPreventDefault);
    bool SendRealMouseEvent(mozilla::WidgetMouseEvent& event);
    bool SendMouseWheelEvent(mozilla::WidgetWheelEvent& event);
    bool SendRealKeyEvent(mozilla::WidgetKeyboardEvent& event);
    bool SendRealTouchEvent(WidgetTouchEvent& event);
    bool SendHandleSingleTap(const CSSIntPoint& aPoint);
    bool SendHandleLongTap(const CSSIntPoint& aPoint);
    bool SendHandleDoubleTap(const CSSIntPoint& aPoint);

    virtual PDocumentRendererParent*
    AllocPDocumentRendererParent(const nsRect& documentRect, const gfxMatrix& transform,
                                 const nsString& bgcolor,
                                 const uint32_t& renderFlags, const bool& flushLayout,
                                 const nsIntSize& renderSize);
    virtual bool DeallocPDocumentRendererParent(PDocumentRendererParent* actor);

    virtual PContentPermissionRequestParent*
    AllocPContentPermissionRequestParent(const nsCString& aType, const nsCString& aAccess, const IPC::Principal& aPrincipal);
    virtual bool DeallocPContentPermissionRequestParent(PContentPermissionRequestParent* actor);

    virtual POfflineCacheUpdateParent* AllocPOfflineCacheUpdateParent(
            const URIParams& aManifestURI,
            const URIParams& aDocumentURI,
            const bool& stickDocument) MOZ_OVERRIDE;
    virtual bool DeallocPOfflineCacheUpdateParent(POfflineCacheUpdateParent* actor);

    bool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHPROMPTPROVIDER
    NS_DECL_NSISECUREBROWSERUI

    void HandleDelayedDialogs();

    static TabParent *GetIMETabParent() { return mIMETabParent; }
    bool HandleQueryContentEvent(mozilla::WidgetQueryContentEvent& aEvent);
    bool SendCompositionEvent(mozilla::WidgetCompositionEvent& event);
    bool SendTextEvent(mozilla::WidgetTextEvent& event);
    bool SendSelectionEvent(mozilla::WidgetSelectionEvent& event);

    static TabParent* GetFrom(nsFrameLoader* aFrameLoader);
    static TabParent* GetFrom(nsIContent* aContent);

    ContentParent* Manager() { return mManager; }

    /**
     * Let managees query if Destroy() is already called so they don't send out
     * messages when the PBrowser actor is being destroyed.
     */
    bool IsDestroyed() const { return mIsDestroyed; }

protected:
    bool ReceiveMessage(const nsString& aMessage,
                        bool aSync,
                        const StructuredCloneData* aCloneData,
                        CpowHolder* aCpows,
                        nsIPrincipal* aPrincipal,
                        InfallibleTArray<nsString>* aJSONRetVal = nullptr);

    virtual bool Recv__delete__() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    virtual PIndexedDBParent* AllocPIndexedDBParent(
                                                  const nsCString& aGroup,
                                                  const nsCString& aASCIIOrigin,
                                                  bool* /* aAllowed */);

    virtual bool DeallocPIndexedDBParent(PIndexedDBParent* aActor);

    virtual bool
    RecvPIndexedDBConstructor(PIndexedDBParent* aActor,
                              const nsCString& aGroup,
                              const nsCString& aASCIIOrigin,
                              bool* aAllowed);

    Element* mFrameElement;
    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;

    struct DelayedDialogData
    {
      DelayedDialogData(PContentDialogParent* aDialog, uint32_t aType,
                        const nsCString& aName,
                        const nsCString& aFeatures,
                        nsIDialogParamBlock* aParams)
      : mDialog(aDialog), mType(aType), mName(aName), mFeatures(aFeatures),
        mParams(aParams) {}

      PContentDialogParent* mDialog;
      uint32_t mType;
      nsCString mName;
      nsCString mFeatures;
      nsCOMPtr<nsIDialogParamBlock> mParams;
    };
    InfallibleTArray<DelayedDialogData*> mDelayedDialogs;

    bool ShouldDelayDialogs();
    bool AllowContentIME();

    virtual PRenderFrameParent* AllocPRenderFrameParent(ScrollingBehavior* aScrolling,
                                                        TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                        uint64_t* aLayersId) MOZ_OVERRIDE;
    virtual bool DeallocPRenderFrameParent(PRenderFrameParent* aFrame) MOZ_OVERRIDE;

    // IME
    static TabParent *mIMETabParent;
    nsString mIMECacheText;
    uint32_t mIMESelectionAnchor;
    uint32_t mIMESelectionFocus;
    bool mIMEComposing;
    bool mIMECompositionEnding;
    // Buffer to store composition text during ResetInputState
    // Compositions in almost all cases are small enough for nsAutoString
    nsAutoString mIMECompositionText;
    uint32_t mIMECompositionStart;
    uint32_t mIMESeqno;

    // The number of event series we're currently capturing.
    int32_t mEventCaptureDepth;

    nsRect mRect;
    nsIntSize mDimensions;
    ScreenOrientation mOrientation;
    float mDPI;
    CSSToLayoutDeviceScale mDefaultScale;
    bool mShown;
    bool mUpdatedDimensions;

private:
    already_AddRefed<nsFrameLoader> GetFrameLoader() const;
    already_AddRefed<nsIWidget> GetWidget() const;
    layout::RenderFrameParent* GetRenderFrame();
    nsRefPtr<ContentParent> mManager;
    void TryCacheDPIAndScale();

    CSSIntPoint AdjustTapToChildWidget(const CSSIntPoint& aPoint);

    // When true, we create a pan/zoom controller for our frame and
    // notify it of input events targeting us.
    bool UseAsyncPanZoom();
    // If we have a render frame currently, notify it that we're about
    // to dispatch |aEvent| to our child.  If there's a relevant
    // transform in place, |aOutEvent| is the transformed |aEvent| to
    // dispatch to content. |aOutTargetGuid| will contain the identifier
    // of the APZC instance that handled the event. aOutTargetGuid may be
    // null but aOutEvent must not be.
    void MaybeForwardEventToRenderFrame(const WidgetInputEvent& aEvent,
                                        ScrollableLayerGuid* aOutTargetGuid,
                                        WidgetInputEvent* aOutEvent);
    // The offset for the child process which is sampled at touch start. This
    // means that the touch events are relative to where the frame was at the
    // start of the touch. We need to look for a better solution to this
    // problem see bug 872911.
    LayoutDeviceIntPoint mChildProcessOffsetAtTouchStart;
    // When true, we've initiated normal shutdown and notified our
    // managing PContent.
    bool mMarkedDestroying;
    // When true, the TabParent is invalid and we should not send IPC messages
    // anymore.
    bool mIsDestroyed;
    // Whether we have already sent a FileDescriptor for the app package.
    bool mAppPackageFileDescriptorSent;
};

} // namespace dom
} // namespace mozilla

#endif

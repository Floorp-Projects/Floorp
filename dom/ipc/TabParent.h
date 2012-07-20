/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "base/basictypes.h"

#include "jsapi.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/PContentDialogParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsCOMPtr.h"
#include "nsIAuthPromptProvider.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsISecureBrowserUI.h"
#include "nsITabParent.h"
#include "nsWeakReference.h"

struct gfxMatrix;
struct JSContext;
struct JSObject;
class nsFrameLoader;
class nsIDOMElement;
class nsIURI;

namespace mozilla {

namespace layers {
struct FrameMetrics;
}

namespace layout {
class RenderFrameParent;
}

namespace dom {

class ContentDialogParent : public PContentDialogParent {};

class TabParent : public PBrowserParent 
                , public nsITabParent 
                , public nsIAuthPromptProvider
                , public nsISecureBrowserUI
{
public:
    TabParent();
    virtual ~TabParent();
    nsIDOMElement* GetOwnerElement() { return mFrameElement; }
    void SetOwnerElement(nsIDOMElement* aElement);
    nsIBrowserDOMWindow *GetBrowserDOMWindow() { return mBrowserDOMWindow; }
    void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow) {
        mBrowserDOMWindow = aBrowserDOMWindow;
    }
 
    void Destroy();

    virtual bool RecvMoveFocus(const bool& aForward);
    virtual bool RecvEvent(const RemoteDOMEvent& aEvent);
    virtual bool RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                            const nsString& aURL,
                                            const nsString& aName,
                                            const nsString& aFeatures,
                                            bool* aOutWindowOpened);
    virtual bool AnswerCreateWindow(PBrowserParent** retval);
    virtual bool RecvSyncMessage(const nsString& aMessage,
                                 const nsString& aJSON,
                                 InfallibleTArray<nsString>* aJSONRetVal);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const nsString& aJSON);
    virtual bool RecvNotifyIMEFocus(const bool& aFocus,
                                    nsIMEUpdatePreference* aPreference,
                                    PRUint32* aSeqno);
    virtual bool RecvNotifyIMETextChange(const PRUint32& aStart,
                                         const PRUint32& aEnd,
                                         const PRUint32& aNewEnd);
    virtual bool RecvNotifyIMESelection(const PRUint32& aSeqno,
                                        const PRUint32& aAnchor,
                                        const PRUint32& aFocus);
    virtual bool RecvNotifyIMETextHint(const nsString& aText);
    virtual bool RecvEndIMEComposition(const bool& aCancel,
                                       nsString* aComposition);
    virtual bool RecvGetInputContext(PRInt32* aIMEEnabled,
                                     PRInt32* aIMEOpen);
    virtual bool RecvSetInputContext(const PRInt32& aIMEEnabled,
                                     const PRInt32& aIMEOpen,
                                     const nsString& aType,
                                     const nsString& aActionHint,
                                     const PRInt32& aCause,
                                     const PRInt32& aFocusChange);
    virtual bool RecvSetCursor(const PRUint32& aValue);
    virtual bool RecvSetBackgroundColor(const nscolor& aValue);
    virtual bool RecvGetDPI(float* aValue);
    virtual bool RecvGetWidgetNativeData(WindowsHandle* aValue);
    virtual PContentDialogParent* AllocPContentDialog(const PRUint32& aType,
                                                      const nsCString& aName,
                                                      const nsCString& aFeatures,
                                                      const InfallibleTArray<int>& aIntParams,
                                                      const InfallibleTArray<nsString>& aStringParams);
    virtual bool DeallocPContentDialog(PContentDialogParent* aDialog)
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
    void Activate();
    void Deactivate();

    /**
     * Is this object active?  That is, was Activate() called more recently than
     * Deactivate()?
     */
    bool Active();

    void SendMouseEvent(const nsAString& aType, float aX, float aY,
                        PRInt32 aButton, PRInt32 aClickCount,
                        PRInt32 aModifiers, bool aIgnoreRootScrollFrame);
    void SendKeyEvent(const nsAString& aType, PRInt32 aKeyCode,
                      PRInt32 aCharCode, PRInt32 aModifiers,
                      bool aPreventDefault);
    bool SendRealMouseEvent(nsMouseEvent& event);
    bool SendMouseScrollEvent(nsMouseScrollEvent& event);
    bool SendRealKeyEvent(nsKeyEvent& event);
    bool SendRealTouchEvent(nsTouchEvent& event);

    virtual PDocumentRendererParent*
    AllocPDocumentRenderer(const nsRect& documentRect, const gfxMatrix& transform,
                           const nsString& bgcolor,
                           const PRUint32& renderFlags, const bool& flushLayout,
                           const nsIntSize& renderSize);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererParent* actor);

    virtual PContentPermissionRequestParent* AllocPContentPermissionRequest(const nsCString& aType, const IPC::URI& uri);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestParent* actor);

    virtual POfflineCacheUpdateParent* AllocPOfflineCacheUpdate(
            const URI& aManifestURI,
            const URI& aDocumentURI,
            const nsCString& aClientID,
            const bool& stickDocument);
    virtual bool DeallocPOfflineCacheUpdate(POfflineCacheUpdateParent* actor);

    JSBool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHPROMPTPROVIDER
    NS_DECL_NSISECUREBROWSERUI

    void HandleDelayedDialogs();

    static TabParent *GetIMETabParent() { return mIMETabParent; }
    bool HandleQueryContentEvent(nsQueryContentEvent& aEvent);
    bool SendCompositionEvent(nsCompositionEvent& event);
    bool SendTextEvent(nsTextEvent& event);
    bool SendSelectionEvent(nsSelectionEvent& event);

    static TabParent* GetFrom(nsFrameLoader* aFrameLoader);
    static TabParent* GetFrom(nsIContent* aContent);

protected:
    bool ReceiveMessage(const nsString& aMessage,
                        bool aSync,
                        const nsString& aJSON,
                        InfallibleTArray<nsString>* aJSONRetVal = nsnull);

    virtual bool Recv__delete__() MOZ_OVERRIDE;

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    virtual PIndexedDBParent* AllocPIndexedDB(const nsCString& aASCIIOrigin,
                                              bool* /* aAllowed */);

    virtual bool DeallocPIndexedDB(PIndexedDBParent* aActor);

    virtual bool
    RecvPIndexedDBConstructor(PIndexedDBParent* aActor,
                              const nsCString& aASCIIOrigin,
                              bool* aAllowed);

    nsIDOMElement* mFrameElement;
    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;

    struct DelayedDialogData
    {
      DelayedDialogData(PContentDialogParent* aDialog, PRUint32 aType,
                        const nsCString& aName,
                        const nsCString& aFeatures,
                        nsIDialogParamBlock* aParams)
      : mDialog(aDialog), mType(aType), mName(aName), mFeatures(aFeatures),
        mParams(aParams) {}

      PContentDialogParent* mDialog;
      PRUint32 mType;
      nsCString mName;
      nsCString mFeatures;
      nsCOMPtr<nsIDialogParamBlock> mParams;
    };
    InfallibleTArray<DelayedDialogData*> mDelayedDialogs;

    bool ShouldDelayDialogs();
    bool AllowContentIME();

    NS_OVERRIDE
    virtual PRenderFrameParent* AllocPRenderFrame(ScrollingBehavior* aScrolling,
                                                  LayersBackend* aBackend,
                                                  int32_t* aMaxTextureSize,
                                                  uint64_t* aLayersId);
    NS_OVERRIDE
    virtual bool DeallocPRenderFrame(PRenderFrameParent* aFrame);

    // IME
    static TabParent *mIMETabParent;
    nsString mIMECacheText;
    PRUint32 mIMESelectionAnchor;
    PRUint32 mIMESelectionFocus;
    bool mIMEComposing;
    bool mIMECompositionEnding;
    // Buffer to store composition text during ResetInputState
    // Compositions in almost all cases are small enough for nsAutoString
    nsAutoString mIMECompositionText;
    PRUint32 mIMECompositionStart;
    PRUint32 mIMESeqno;

    float mDPI;
    bool mActive;
    bool mShown;

private:
    already_AddRefed<nsFrameLoader> GetFrameLoader() const;
    already_AddRefed<nsIWidget> GetWidget() const;
    layout::RenderFrameParent* GetRenderFrame();
    void TryCacheDPI();
    // Return true iff this TabParent was created for a mozbrowser
    // frame.
    bool IsForMozBrowser();
    // When true, we create a pan/zoom controller for our frame and
    // notify it of input events targeting us.
    bool UseAsyncPanZoom();
    // If we have a render frame currently, notify it that we're about
    // to dispatch |aEvent| to our child.  If there's a relevant
    // transform in place, |aOutEvent| is the transformed |aEvent| to
    // dispatch to content.
    void MaybeForwardEventToRenderFrame(const nsInputEvent& aEvent,
                                        nsInputEvent* aOutEvent);
};

} // namespace dom
} // namespace mozilla

#endif

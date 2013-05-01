/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TabChild_h
#define mozilla_dom_TabChild_h

#ifndef _IMPL_NS_LAYOUT
#include "mozilla/dom/PBrowserChild.h"
#endif
#ifdef DEBUG
#include "PCOMContentPermissionRequestChild.h"
#endif /* DEBUG */
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIWidget.h"
#include "nsIDOMEventListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWindowProvider.h"
#include "nsIXPCScriptable.h"
#include "nsIClassInfo.h"
#include "jsapi.h"
#include "nsIXPConnect.h"
#include "nsIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsNetUtil.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsIWebProgressListener.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDialogCreator.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindowUtils.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsWeakReference.h"
#include "nsITabChild.h"
#include "mozilla/Attributes.h"
#include "FrameMetrics.h"
#include "ProcessUtils.h"
#include "mozilla/dom/TabContext.h"

struct gfxMatrix;
class nsICachedFileDescriptorListener;

namespace mozilla {
namespace layout {
class RenderFrameChild;
}

namespace layers {
struct TextureFactoryIdentifier;
}

namespace dom {

class TabChild;
class PContentDialogChild;
class ClonedMessageData;

class TabChildGlobal : public nsDOMEventTargetHelper,
                       public nsIContentFrameMessageManager,
                       public nsIScriptObjectPrincipal,
                       public nsIScriptContextPrincipal
{
public:
  TabChildGlobal(TabChild* aTabChild);
  void Init();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)
  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             const JS::Value& aObject,
                             JSContext* aCx,
                             uint8_t aArgc,
                             JS::Value* aRetval)
  {
    return mMessageManager
      ? mMessageManager->SendSyncMessage(aMessageName, aObject, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(nsIDOMWindow** aContent);
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell);
  NS_IMETHOD Dump(const nsAString& aStr)
  {
    return mMessageManager ? mMessageManager->Dump(aStr) : NS_OK;
  }
  NS_IMETHOD PrivateNoteIntentionalCrash();
  NS_IMETHOD Btoa(const nsAString& aBinaryData,
                  nsAString& aAsciiBase64String);
  NS_IMETHOD Atob(const nsAString& aAsciiString,
                  nsAString& aBinaryData);

  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return nsDOMEventTargetHelper::AddEventListener(aType, aListener,
                                                    aUseCapture, false, 2);
  }
  using nsDOMEventTargetHelper::AddEventListener;
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture, bool aWantsUntrusted,
                              uint8_t optional_argc)
  {
    return nsDOMEventTargetHelper::AddEventListener(aType, aListener,
                                                    aUseCapture,
                                                    aWantsUntrusted,
                                                    optional_argc);
  }

  virtual nsIScriptObjectPrincipal* GetObjectPrincipal() { return this; }
  virtual JSContext* GetJSContextForEventHandlers();
  virtual nsIPrincipal* GetPrincipal();

  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;
  TabChild* mTabChild;
};

class ContentListener MOZ_FINAL : public nsIDOMEventListener
{
public:
  ContentListener(TabChild* aTabChild) : mTabChild(aTabChild) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
protected:
  TabChild* mTabChild;
};

class TabChild : public PBrowserChild,
                 public nsFrameScriptExecutor,
                 public nsIWebBrowserChrome2,
                 public nsIEmbeddingSiteWindow,
                 public nsIWebBrowserChromeFocus,
                 public nsIInterfaceRequestor,
                 public nsIWindowProvider,
                 public nsIDOMEventListener,
                 public nsIWebProgressListener,
                 public nsSupportsWeakReference,
                 public nsIDialogCreator,
                 public nsITabChild,
                 public nsIObserver,
                 public ipc::MessageManagerCallback,
                 public TabContext
{
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;
    typedef mozilla::layout::RenderFrameChild RenderFrameChild;
    typedef mozilla::layout::ScrollingBehavior ScrollingBehavior;

public:
    /** 
     * This is expected to be called off the critical path to content
     * startup.  This is an opportunity to load things that are slow
     * on the critical path.
     */
    static void PreloadSlowThings();

    /** Return a TabChild with the given attributes. */
    static already_AddRefed<TabChild> 
    Create(const TabContext& aContext, uint32_t aChromeFlags);

    virtual ~TabChild();

    bool IsRootContentDocument();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER
    NS_DECL_NSIDOMEVENTLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIDIALOGCREATOR
    NS_DECL_NSITABCHILD
    NS_DECL_NSIOBSERVER

    /**
     * MessageManagerCallback methods that we override.
     */
    virtual bool DoSendSyncMessage(const nsAString& aMessage,
                                   const mozilla::dom::StructuredCloneData& aData,
                                   InfallibleTArray<nsString>* aJSONRetVal);
    virtual bool DoSendAsyncMessage(const nsAString& aMessage,
                                    const mozilla::dom::StructuredCloneData& aData);

    virtual bool RecvLoadURL(const nsCString& uri);
    virtual bool RecvCacheFileDescriptor(const nsString& aPath,
                                         const FileDescriptor& aFileDescriptor)
                                         MOZ_OVERRIDE;
    virtual bool RecvShow(const nsIntSize& size);
    virtual bool RecvUpdateDimensions(const nsRect& rect, const nsIntSize& size, const ScreenOrientation& orientation);
    virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);
    virtual bool RecvHandleDoubleTap(const nsIntPoint& aPoint);
    virtual bool RecvHandleSingleTap(const nsIntPoint& aPoint);
    virtual bool RecvHandleLongTap(const nsIntPoint& aPoint);
    virtual bool RecvActivate();
    virtual bool RecvDeactivate();
    virtual bool RecvMouseEvent(const nsString& aType,
                                const float&    aX,
                                const float&    aY,
                                const int32_t&  aButton,
                                const int32_t&  aClickCount,
                                const int32_t&  aModifiers,
                                const bool&     aIgnoreRootScrollFrame);
    virtual bool RecvRealMouseEvent(const nsMouseEvent& event);
    virtual bool RecvRealKeyEvent(const nsKeyEvent& event);
    virtual bool RecvMouseWheelEvent(const mozilla::widget::WheelEvent& event);
    virtual bool RecvRealTouchEvent(const nsTouchEvent& event);
    virtual bool RecvRealTouchMoveEvent(const nsTouchEvent& event);
    virtual bool RecvKeyEvent(const nsString& aType,
                              const int32_t&  aKeyCode,
                              const int32_t&  aCharCode,
                              const int32_t&  aModifiers,
                              const bool&     aPreventDefault);
    virtual bool RecvCompositionEvent(const nsCompositionEvent& event);
    virtual bool RecvTextEvent(const nsTextEvent& event);
    virtual bool RecvSelectionEvent(const nsSelectionEvent& event);
    virtual bool RecvActivateFrameEvent(const nsString& aType, const bool& capture);
    virtual bool RecvLoadRemoteScript(const nsString& aURL);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const ClonedMessageData& aData);

    virtual PDocumentRendererChild*
    AllocPDocumentRenderer(const nsRect& documentRect, const gfxMatrix& transform,
                           const nsString& bgcolor,
                           const uint32_t& renderFlags, const bool& flushLayout,
                           const nsIntSize& renderSize);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererChild* actor);
    virtual bool RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                                  const nsRect& documentRect,
                                                  const gfxMatrix& transform,
                                                  const nsString& bgcolor,
                                                  const uint32_t& renderFlags,
                                                  const bool& flushLayout,
                                                  const nsIntSize& renderSize);

    virtual PContentDialogChild* AllocPContentDialog(const uint32_t&,
                                                     const nsCString&,
                                                     const nsCString&,
                                                     const InfallibleTArray<int>&,
                                                     const InfallibleTArray<nsString>&);
    virtual bool DeallocPContentDialog(PContentDialogChild* aDialog);
    static void ParamsToArrays(nsIDialogParamBlock* aParams,
                               InfallibleTArray<int>& aIntParams,
                               InfallibleTArray<nsString>& aStringParams);
    static void ArraysToParams(const InfallibleTArray<int>& aIntParams,
                               const InfallibleTArray<nsString>& aStringParams,
                               nsIDialogParamBlock* aParams);

#ifdef DEBUG
    virtual PContentPermissionRequestChild* SendPContentPermissionRequestConstructor(PContentPermissionRequestChild* aActor,
                                                                                     const nsCString& aType,
                                                                                     const nsCString& aAccess,
                                                                                     const IPC::Principal& aPrincipal)
    {
      PCOMContentPermissionRequestChild* child = static_cast<PCOMContentPermissionRequestChild*>(aActor);
      PContentPermissionRequestChild* request = PBrowserChild::SendPContentPermissionRequestConstructor(aActor, aType, aAccess, aPrincipal);
      child->mIPCOpen = true;
      return request;
    }
#endif /* DEBUG */

    virtual PContentPermissionRequestChild* AllocPContentPermissionRequest(const nsCString& aType,
                                                                           const nsCString& aAccess,
                                                                           const IPC::Principal& aPrincipal);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor);

    virtual POfflineCacheUpdateChild* AllocPOfflineCacheUpdate(
            const URIParams& manifestURI,
            const URIParams& documentURI,
            const bool& stickDocument);
    virtual bool DeallocPOfflineCacheUpdate(POfflineCacheUpdateChild* offlineCacheUpdate);

    nsIWebNavigation* WebNavigation() { return mWebNav; }

    JSContext* GetJSContext() { return mCx; }

    nsIPrincipal* GetPrincipal() { return mPrincipal; }

    /** Return the DPI of the widget this TabChild draws to. */
    void GetDPI(float* aDPI);
    void GetDefaultScale(double *aScale);

    gfxSize GetZoom() { return mLastMetrics.mZoom; }

    ScreenOrientation GetOrientation() { return mOrientation; }

    void SetBackgroundColor(const nscolor& aColor);

    void NotifyPainted();

    bool IsAsyncPanZoomEnabled();

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

protected:
    virtual PRenderFrameChild* AllocPRenderFrame(ScrollingBehavior* aScrolling,
                                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                 uint64_t* aLayersId) MOZ_OVERRIDE;
    virtual bool DeallocPRenderFrame(PRenderFrameChild* aFrame) MOZ_OVERRIDE;
    virtual bool RecvDestroy() MOZ_OVERRIDE;

    nsEventStatus DispatchWidgetEvent(nsGUIEvent& event);

    virtual PIndexedDBChild* AllocPIndexedDB(const nsCString& aASCIIOrigin,
                                             bool* /* aAllowed */);

    virtual bool DeallocPIndexedDB(PIndexedDBChild* aActor);

private:
    /**
     * Create a new TabChild object.
     *
     * |aOwnOrContainingAppId| is the app-id of our frame or of the closest app
     * frame in the hierarchy which contains us.
     *
     * |aIsBrowserElement| indicates whether we're a browser (but not an app).
     */
    TabChild(const TabContext& aContext, uint32_t aChromeFlags);

    nsresult Init();

    // Notify others that our TabContext has been updated.  (At the moment, this
    // sets the appropriate app-id and is-browser flags on our docshell.)
    //
    // You should call this after calling TabContext::SetTabContext().  We also
    // call this during Init().
    void NotifyTabContextUpdated();

    bool UseDirectCompositor();

    void ActorDestroy(ActorDestroyReason why);

    enum FrameScriptLoading { DONT_LOAD_SCRIPTS, DEFAULT_LOAD_SCRIPTS };
    bool InitTabChildGlobal(FrameScriptLoading aScriptLoading = DEFAULT_LOAD_SCRIPTS);
    bool InitRenderingState();
    void DestroyWindow();
    void SetProcessNameToAppName();

    // Call RecvShow(nsIntSize(0, 0)) and block future calls to RecvShow().
    void DoFakeShow();

    // Wrapper for nsIDOMWindowUtils.setCSSViewport(). This updates some state
    // variables local to this class before setting it.
    void SetCSSViewport(float aX, float aY);

    // Recalculates the display state, including the CSS
    // viewport. This should be called whenever we believe the
    // viewport data on a document may have changed. If it didn't
    // change, this function doesn't do anything.  However, it should
    // not be called all the time as it is fairly expensive.
    void HandlePossibleViewportChange();

    // Wraps up a JSON object as a structured clone and sends it to the browser
    // chrome script.
    //
    // XXX/bug 780335: Do the work the browser chrome script does in C++ instead
    // so we don't need things like this.
    void DispatchMessageManagerMessage(const nsAString& aMessageName,
                                       const nsACString& aJSONData);

    void DispatchSynthesizedMouseEvent(uint32_t aMsg, uint64_t aTime,
                                       const nsIntPoint& aRefPoint);

    // These methods are used for tracking synthetic mouse events
    // dispatched for compatibility.  On each touch event, we
    // UpdateTapState().  If we've detected that the current gesture
    // isn't a tap, then we CancelTapTracking().  In the meantime, we
    // may detect a context-menu event, and if so we
    // FireContextMenuEvent().
    void FireContextMenuEvent();
    void CancelTapTracking();
    void UpdateTapState(const nsTouchEvent& aEvent, nsEventStatus aStatus);

    nsresult
    BrowserFrameProvideWindow(nsIDOMWindow* aOpener,
                              nsIURI* aURI,
                              const nsAString& aName,
                              const nsACString& aFeatures,
                              bool* aWindowIsNew,
                              nsIDOMWindow** aReturn);

    nsIDOMWindowUtils* GetDOMWindowUtils()
    {
        nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
        nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
        return utils;
    }

    class CachedFileDescriptorInfo;
    class CachedFileDescriptorCallbackRunnable;

    TextureFactoryIdentifier mTextureFactoryIdentifier;
    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsCOMPtr<nsIWidget> mWidget;
    nsCOMPtr<nsIURI> mLastURI;
    FrameMetrics mLastMetrics;
    RenderFrameChild* mRemoteFrame;
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
    uint32_t mChromeFlags;
    nsIntRect mOuterRect;
    nsIntSize mInnerSize;
    // When we're tracking a possible tap gesture, this is the "down"
    // point of the touchstart.
    nsIntPoint mGestureDownPoint;
    // The touch identifier of the active gesture.
    int32_t mActivePointerId;
    // A timer task that fires if the tap-hold timeout is exceeded by
    // the touch we're tracking.  That is, if touchend or a touchmove
    // that exceeds the gesture threshold doesn't happen.
    CancelableTask* mTapHoldTimer;
    // At present only 1 of these is really expected.
    nsAutoTArray<nsAutoPtr<CachedFileDescriptorInfo>, 1>
        mCachedFileDescriptorInfos;
    float mOldViewportWidth;
    nscolor mLastBackgroundColor;
    ScrollingBehavior mScrolling;
    bool mDidFakeShow;
    bool mNotified;
    bool mContentDocumentIsDisplayed;
    bool mTriedBrowserInit;
    ScreenOrientation mOrientation;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

inline TabChild*
GetTabChildFrom(nsIDocShell* aDocShell)
{
    nsCOMPtr<nsITabChild> tc = do_GetInterface(aDocShell);
    return static_cast<TabChild*>(tc.get());
}

inline TabChild*
GetTabChildFrom(nsIPresShell* aPresShell)
{
    nsIDocument* doc = aPresShell->GetDocument();
    if (!doc) {
        return nullptr;
    }
    nsCOMPtr<nsISupports> container = doc->GetContainer();
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
    return GetTabChildFrom(docShell);
}

inline TabChild*
GetTabChildFrom(nsIDOMWindow* aWindow)
{
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow);
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(webNav);
    return GetTabChildFrom(docShell);
}

}
}

#endif // mozilla_dom_TabChild_h

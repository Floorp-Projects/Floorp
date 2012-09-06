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
#include "nsIDOMEventTarget.h"
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
#include "nsDOMEventTargetHelper.h"
#include "nsIDialogCreator.h"
#include "nsIDialogParamBlock.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsWeakReference.h"
#include "nsITabChild.h"
#include "mozilla/Attributes.h"
#include "FrameMetrics.h"

struct gfxMatrix;

namespace mozilla {
namespace layout {
class RenderFrameChild;
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
                             const jsval& aObject,
                             JSContext* aCx,
                             uint8_t aArgc,
                             jsval* aRetval)
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
                 public nsSupportsWeakReference,
                 public nsIDialogCreator,
                 public nsITabChild,
                 public nsIObserver
{
    typedef mozilla::layout::RenderFrameChild RenderFrameChild;
    typedef mozilla::dom::ClonedMessageData ClonedMessageData;

public:
    /** 
     * This is expected to be called off the critical path to content
     * startup.  This is an opportunity to load things that are slow
     * on the critical path.
     */
    static void PreloadSlowThings();

    /** Return a TabChild with the given attributes. */
    static already_AddRefed<TabChild> 
    Create(uint32_t aChromeFlags, bool aIsBrowserElement, uint32_t aAppId);

    virtual ~TabChild();

    uint32_t GetAppId() { return mAppId; }

    bool IsRootContentDocument();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER
    NS_DECL_NSIDIALOGCREATOR
    NS_DECL_NSITABCHILD
    NS_DECL_NSIOBSERVER

    virtual bool RecvLoadURL(const nsCString& uri);
    virtual bool RecvShow(const nsIntSize& size);
    virtual bool RecvUpdateDimensions(const nsRect& rect, const nsIntSize& size);
    virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);
    virtual bool RecvHandleDoubleTap(const nsIntPoint& aPoint);
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
                                                                                     const IPC::Principal& aPrincipal)
    {
      PCOMContentPermissionRequestChild* child = static_cast<PCOMContentPermissionRequestChild*>(aActor);
      PContentPermissionRequestChild* request = PBrowserChild::SendPContentPermissionRequestConstructor(aActor, aType, aPrincipal);
      child->mIPCOpen = true;
      return request;
    }
#endif /* DEBUG */

    virtual PContentPermissionRequestChild* AllocPContentPermissionRequest(const nsCString& aType, const IPC::Principal& aPrincipal);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor);

    virtual POfflineCacheUpdateChild* AllocPOfflineCacheUpdate(const URIParams& manifestURI,
            const URIParams& documentURI,
            const nsCString& clientID,
            const bool& stickDocument);
    virtual bool DeallocPOfflineCacheUpdate(POfflineCacheUpdateChild* offlineCacheUpdate);

    nsIWebNavigation* WebNavigation() { return mWebNav; }

    JSContext* GetJSContext() { return mCx; }

    nsIPrincipal* GetPrincipal() { return mPrincipal; }

    /** Return the DPI of the widget this TabChild draws to. */
    void GetDPI(float* aDPI);

    void SetBackgroundColor(const nscolor& aColor);

    void NotifyPainted();

    bool IsAsyncPanZoomEnabled();

protected:
    virtual PRenderFrameChild* AllocPRenderFrame(ScrollingBehavior* aScrolling,
                                                 LayersBackend* aBackend,
                                                 int32_t* aMaxTextureSize,
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
     * |aIsBrowserElement| indicates whether the tab is inside an <iframe mozbrowser>.
     * |aAppId| is the app id of the app containing this tab. If the tab isn't
     * contained in an app, aAppId will be nsIScriptSecurityManager::NO_APP_ID.
     */
    TabChild(uint32_t aChromeFlags, bool aIsBrowserElement, uint32_t aAppId);

    nsresult Init();

    void SetAppBrowserConfig(bool aIsBrowserElement, uint32_t aAppId);

    bool UseDirectCompositor();

    void ActorDestroy(ActorDestroyReason why);

    enum FrameScriptLoading { DONT_LOAD_SCRIPTS, DEFAULT_LOAD_SCRIPTS };
    bool InitTabChildGlobal(FrameScriptLoading aScriptLoading = DEFAULT_LOAD_SCRIPTS);
    bool InitRenderingState();
    void DestroyWindow();

    // Call RecvShow(nsIntSize(0, 0)) and block future calls to RecvShow().
    void DoFakeShow();

    // Wraps up a JSON object as a structured clone and sends it to the browser
    // chrome script.
    //
    // XXX/bug 780335: Do the work the browser chrome script does in C++ instead
    // so we don't need things like this.
    void DispatchMessageManagerMessage(const nsAString& aMessageName,
                                       const nsACString& aJSONData);

    nsresult
    BrowserFrameProvideWindow(nsIDOMWindow* aOpener,
                              nsIURI* aURI,
                              const nsAString& aName,
                              const nsACString& aFeatures,
                              bool* aWindowIsNew,
                              nsIDOMWindow** aReturn);

    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsCOMPtr<nsIWidget> mWidget;
    RenderFrameChild* mRemoteFrame;
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
    uint32_t mChromeFlags;
    nsIntRect mOuterRect;
    nscolor mLastBackgroundColor;
    ScrollingBehavior mScrolling;
    uint32_t mAppId;
    bool mDidFakeShow;
    bool mIsBrowserElement;
    bool mNotified;
    bool mTriedBrowserInit;

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

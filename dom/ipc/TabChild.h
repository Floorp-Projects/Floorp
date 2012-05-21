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
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow2.h"
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
#include "nsWeakReference.h"
#include "nsITabChild.h"

struct gfxMatrix;

namespace mozilla {
namespace layout {
class RenderFrameChild;
}

namespace dom {

class TabChild;
class PContentDialogChild;

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
  NS_FORWARD_SAFE_NSIFRAMEMESSAGEMANAGER(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             const jsval& aObject,
                             JSContext* aCx,
                             PRUint8 aArgc,
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
                              PRUint8 optional_argc)
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

class ContentListener : public nsIDOMEventListener
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
                 public nsIEmbeddingSiteWindow2,
                 public nsIWebBrowserChromeFocus,
                 public nsIInterfaceRequestor,
                 public nsIWindowProvider,
                 public nsSupportsWeakReference,
                 public nsIDialogCreator,
                 public nsITabChild
{
    typedef mozilla::layout::RenderFrameChild RenderFrameChild;

public:
    TabChild(PRUint32 aChromeFlags);
    virtual ~TabChild();
    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIEMBEDDINGSITEWINDOW2
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER
    NS_DECL_NSIDIALOGCREATOR

    virtual bool RecvLoadURL(const nsCString& uri);
    virtual bool RecvShow(const nsIntSize& size);
    virtual bool RecvUpdateDimensions(const nsRect& rect, const nsIntSize& size);
    virtual bool RecvActivate();
    virtual bool RecvDeactivate();
    virtual bool RecvMouseEvent(const nsString& aType,
                                const float&    aX,
                                const float&    aY,
                                const PRInt32&  aButton,
                                const PRInt32&  aClickCount,
                                const PRInt32&  aModifiers,
                                const bool&     aIgnoreRootScrollFrame);
    virtual bool RecvRealMouseEvent(const nsMouseEvent& event);
    virtual bool RecvRealKeyEvent(const nsKeyEvent& event);
    virtual bool RecvMouseScrollEvent(const nsMouseScrollEvent& event);
    virtual bool RecvKeyEvent(const nsString& aType,
                              const PRInt32&  aKeyCode,
                              const PRInt32&  aCharCode,
                              const PRInt32&  aModifiers,
                              const bool&     aPreventDefault);
    virtual bool RecvCompositionEvent(const nsCompositionEvent& event);
    virtual bool RecvTextEvent(const nsTextEvent& event);
    virtual bool RecvSelectionEvent(const nsSelectionEvent& event);
    virtual bool RecvActivateFrameEvent(const nsString& aType, const bool& capture);
    virtual bool RecvLoadRemoteScript(const nsString& aURL);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const nsString& aJSON);

    virtual PDocumentRendererChild*
    AllocPDocumentRenderer(const nsRect& documentRect, const gfxMatrix& transform,
                           const nsString& bgcolor,
                           const PRUint32& renderFlags, const bool& flushLayout,
                           const nsIntSize& renderSize);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererChild* actor);
    virtual bool RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                                  const nsRect& documentRect,
                                                  const gfxMatrix& transform,
                                                  const nsString& bgcolor,
                                                  const PRUint32& renderFlags,
                                                  const bool& flushLayout,
                                                  const nsIntSize& renderSize);

    virtual PContentDialogChild* AllocPContentDialog(const PRUint32&,
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

    virtual PContentPermissionRequestChild* AllocPContentPermissionRequest(const nsCString& aType, const IPC::URI& uri);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor);

    virtual POfflineCacheUpdateChild* AllocPOfflineCacheUpdate(const URI& manifestURI,
            const URI& documentURI,
            const nsCString& clientID,
            const bool& stickDocument);
    virtual bool DeallocPOfflineCacheUpdate(POfflineCacheUpdateChild* offlineCacheUpdate);

    nsIWebNavigation* WebNavigation() { return mWebNav; }

    JSContext* GetJSContext() { return mCx; }

    nsIPrincipal* GetPrincipal() { return mPrincipal; }

    void SetBackgroundColor(const nscolor& aColor);
protected:
    NS_OVERRIDE
    virtual PRenderFrameChild* AllocPRenderFrame();
    NS_OVERRIDE
    virtual bool DeallocPRenderFrame(PRenderFrameChild* aFrame);
    NS_OVERRIDE
    virtual bool RecvDestroy();

    bool DispatchWidgetEvent(nsGUIEvent& event);

private:
    void ActorDestroy(ActorDestroyReason why);

    bool InitTabChildGlobal();
    bool InitWidget(const nsIntSize& size);
    void DestroyWindow();

    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsCOMPtr<nsIWidget> mWidget;
    RenderFrameChild* mRemoteFrame;
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
    PRUint32 mChromeFlags;
    nsIntRect mOuterRect;
    nscolor mLastBackgroundColor;

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
        return nsnull;
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

/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_tabs_TabChild_h
#define mozilla_tabs_TabChild_h

#ifndef _IMPL_NS_LAYOUT
#include "mozilla/dom/PBrowserChild.h"
#endif
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgressListener2.h"
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
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)
  NS_FORWARD_SAFE_NSIFRAMEMESSAGEMANAGER(mMessageManager)
  NS_IMETHOD SendSyncMessage()
  {
    return mMessageManager ? mMessageManager->SendSyncMessage()
                           : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(nsIDOMWindow** aContent);
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell);
  NS_IMETHOD Dump(const nsAString& aStr)
  {
    return mMessageManager ? mMessageManager->Dump(aStr) : NS_OK;
  }

  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              PRBool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return nsDOMEventTargetHelper::AddEventListener(aType, aListener,
                                                    aUseCapture, PR_FALSE, 1);
  }
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              PRBool aUseCapture, PRBool aWantsUntrusted,
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
                 public nsIWebProgressListener2,
                 public nsIWebBrowserChrome2,
                 public nsIEmbeddingSiteWindow2,
                 public nsIWebBrowserChromeFocus,
                 public nsIInterfaceRequestor,
                 public nsIWindowProvider,
                 public nsSupportsWeakReference,
                 public nsIDialogCreator,
                 public nsITabChild
{
public:
    TabChild(PRUint32 aChromeFlags);
    virtual ~TabChild();
    bool DestroyWidget();
    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIWEBPROGRESSLISTENER2
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIEMBEDDINGSITEWINDOW2
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER
    NS_DECL_NSIDIALOGCREATOR

    virtual bool RecvCreateWidget(const MagicWindowHandle& parentWidget);
    virtual bool RecvLoadURL(const nsCString& uri);
    virtual bool RecvMove(const PRUint32& x,
                          const PRUint32& y,
                          const PRUint32& width,
                          const PRUint32& height);
    virtual bool RecvActivate();
    virtual bool RecvMouseEvent(const nsString& aType,
                                const float&    aX,
                                const float&    aY,
                                const PRInt32&  aButton,
                                const PRInt32&  aClickCount,
                                const PRInt32&  aModifiers,
                                const bool&     aIgnoreRootScrollFrame);
    virtual bool RecvKeyEvent(const nsString& aType,
                              const PRInt32&  aKeyCode,
                              const PRInt32&  aCharCode,
                              const PRInt32&  aModifiers,
                              const bool&     aPreventDefault);
    virtual bool RecvCompositionEvent(const nsCompositionEvent& event);
    virtual bool RecvTextEvent(const nsTextEvent& event);
    virtual bool RecvQueryContentEvent(const nsQueryContentEvent& event);
    virtual bool RecvSelectionEvent(const nsSelectionEvent& event);
    virtual bool RecvActivateFrameEvent(const nsString& aType, const bool& capture);
    virtual bool RecvLoadRemoteScript(const nsString& aURL);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const nsString& aJSON);
    virtual mozilla::ipc::PDocumentRendererChild* AllocPDocumentRenderer(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererChild* actor);
    virtual bool RecvPDocumentRendererConstructor(
            mozilla::ipc::PDocumentRendererChild *__a,
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush);
    virtual PContentDialogChild* AllocPContentDialog(const PRUint32&,
                                                     const nsCString&,
                                                     const nsCString&,
                                                     const nsTArray<int>&,
                                                     const nsTArray<nsString>&);
    virtual bool DeallocPContentDialog(PContentDialogChild* aDialog);
    virtual PExternalHelperAppChild *AllocPExternalHelperApp(
            const IPC::URI& uri,
            const nsCString& aMimeContentType,
            const bool& aForceSave,
            const PRInt64& aContentLength);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppChild *aService);
    static void ParamsToArrays(nsIDialogParamBlock* aParams,
                               nsTArray<int>& aIntParams,
                               nsTArray<nsString>& aStringParams);
    static void ArraysToParams(const nsTArray<int>& aIntParams,
                               const nsTArray<nsString>& aStringParams,
                               nsIDialogParamBlock* aParams);

    virtual PDocumentRendererShmemChild* AllocPDocumentRendererShmem(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            Shmem& buf);
    virtual bool DeallocPDocumentRendererShmem(PDocumentRendererShmemChild* actor);
    virtual bool RecvPDocumentRendererShmemConstructor(
            PDocumentRendererShmemChild *__a,
            const PRInt32& aX,
            const PRInt32& aY,
            const PRInt32& aW,
            const PRInt32& aH,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            Shmem& aBuf);

    virtual PDocumentRendererNativeIDChild* AllocPDocumentRendererNativeID(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            const PRUint32& nativeID);
    virtual bool DeallocPDocumentRendererNativeID(PDocumentRendererNativeIDChild* actor);
    virtual bool RecvPDocumentRendererNativeIDConstructor(
            PDocumentRendererNativeIDChild *__a,
            const PRInt32& aX,
            const PRInt32& aY,
            const PRInt32& aW,
            const PRInt32& aH,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            const PRUint32& aNativeID);

    virtual PContentPermissionRequestChild* AllocPContentPermissionRequest(const nsCString& aType, const IPC::URI& uri);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor);

    nsIWebNavigation* WebNavigation() { return mWebNav; }

    JSContext* GetJSContext() { return mCx; }

    nsIPrincipal* GetPrincipal() { return mPrincipal; }

protected:
    NS_OVERRIDE
    virtual bool RecvDestroy();

    bool DispatchWidgetEvent(nsGUIEvent& event);

private:
    void ActorDestroy(ActorDestroyReason why);

    bool InitTabChildGlobal();

    nsCOMPtr<nsIWebNavigation> mWebNav;
    nsRefPtr<TabChildGlobal> mTabChildGlobal;
    PRUint32 mChromeFlags;

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

}
}

#endif // mozilla_tabs_TabChild_h

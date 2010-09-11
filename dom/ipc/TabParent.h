/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef mozilla_tabs_TabParent_h
#define mozilla_tabs_TabParent_h

#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/PContentDialogParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/dom/PExternalHelperApp.h"

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsITabParent.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIDialogParamBlock.h"
#include "nsIAuthPromptProvider.h"
#include "nsISSLStatusProvider.h"
#include "nsISecureBrowserUI.h"

class nsFrameLoader;
class nsIURI;
class nsIDOMElement;
struct gfxMatrix;

struct JSContext;
struct JSObject;

namespace mozilla {
namespace dom {
struct TabParentListenerInfo 
{
  TabParentListenerInfo(nsIWeakReference *aListener, unsigned long aNotifyMask)
    : mWeakListener(aListener), mNotifyMask(aNotifyMask)
  {
  }

  TabParentListenerInfo(const TabParentListenerInfo& obj)
    : mWeakListener(obj.mWeakListener), mNotifyMask(obj.mNotifyMask) 
  {
  }

  nsWeakPtr mWeakListener;

  PRUint32 mNotifyMask;
};

inline    
bool operator==(const TabParentListenerInfo& lhs, const TabParentListenerInfo& rhs)
{
  return &lhs == &rhs;
}

class ContentDialogParent : public PContentDialogParent {};

class TabParent : public PBrowserParent 
                , public nsITabParent 
                , public nsIWebProgress
                , public nsIAuthPromptProvider
                , public nsISecureBrowserUI
                , public nsISSLStatusProvider
{
public:
    TabParent();
    virtual ~TabParent();
    nsIDOMElement* GetOwnerElement() { return mFrameElement; }
    void SetOwnerElement(nsIDOMElement* aElement) { mFrameElement = aElement; }
    nsIBrowserDOMWindow *GetBrowserDOMWindow() { return mBrowserDOMWindow; }
    void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow) {
        mBrowserDOMWindow = aBrowserDOMWindow;
    }
 
    virtual bool RecvMoveFocus(const bool& aForward);
    virtual bool RecvEvent(const RemoteDOMEvent& aEvent);
    virtual bool RecvNotifyProgressChange(const PRInt64& aProgress,
                                          const PRInt64& aProgressMax,
                                          const PRInt64& aTotalProgress,
                                          const PRInt64& aMaxTotalProgress);
    virtual bool RecvNotifyStateChange(const PRUint32& aStateFlags,
                                       const nsresult& aStatus);
    virtual bool RecvNotifyLocationChange(const nsCString& aUri);
    virtual bool RecvNotifyStatusChange(const nsresult& status,
                                        const nsString& message);
    virtual bool RecvNotifySecurityChange(const PRUint32& aState,
                                          const PRBool& aUseSSLStatusObject,
                                          const nsString& aTooltip,
                                          const nsCString& aSecInfoAsString);

    virtual bool RecvRefreshAttempted(const nsCString& aURI,
                                      const PRInt32& aMillis,
                                      const bool& aSameURI,
                                      bool* aAllowRefresh);

    virtual bool AnswerCreateWindow(PBrowserParent** retval);
    virtual bool RecvSyncMessage(const nsString& aMessage,
                                 const nsString& aJSON,
                                 nsTArray<nsString>* aJSONRetVal);
    virtual bool RecvAsyncMessage(const nsString& aMessage,
                                  const nsString& aJSON);
    virtual bool RecvQueryContentResult(const nsQueryContentEvent& event);
    virtual PContentDialogParent* AllocPContentDialog(const PRUint32& aType,
                                                      const nsCString& aName,
                                                      const nsCString& aFeatures,
                                                      const nsTArray<int>& aIntParams,
                                                      const nsTArray<nsString>& aStringParams);
    virtual bool DeallocPContentDialog(PContentDialogParent* aDialog)
    {
      delete aDialog;
      return true;
    }

    virtual PExternalHelperAppParent* AllocPExternalHelperApp(
            const IPC::URI& uri,
            const nsCString& aMimeContentType,
            const bool& aForceSave,
            const PRInt64& aContentLength);
    virtual bool DeallocPExternalHelperApp(PExternalHelperAppParent* aService);

    void LoadURL(nsIURI* aURI);
    void Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height);
    void Activate();
    void SendMouseEvent(const nsAString& aType, float aX, float aY,
                        PRInt32 aButton, PRInt32 aClickCount,
                        PRInt32 aModifiers, PRBool aIgnoreRootScrollFrame);
    void SendKeyEvent(const nsAString& aType, PRInt32 aKeyCode,
                      PRInt32 aCharCode, PRInt32 aModifiers,
                      PRBool aPreventDefault);

    virtual mozilla::ipc::PDocumentRendererParent* AllocPDocumentRenderer(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererParent* actor);

    virtual mozilla::ipc::PDocumentRendererShmemParent* AllocPDocumentRendererShmem(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            Shmem& buf);
    virtual bool DeallocPDocumentRendererShmem(PDocumentRendererShmemParent* actor);

    virtual mozilla::ipc::PDocumentRendererNativeIDParent* AllocPDocumentRendererNativeID(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush,
            const gfxMatrix& aMatrix,
            const PRUint32& nativeID);
    virtual bool DeallocPDocumentRendererNativeID(PDocumentRendererNativeIDParent* actor);


    virtual PContentPermissionRequestParent* AllocPContentPermissionRequest(const nsCString& aType, const IPC::URI& uri);
    virtual bool DeallocPContentPermissionRequest(PContentPermissionRequestParent* actor);

    JSBool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESS
    NS_DECL_NSIAUTHPROMPTPROVIDER
    NS_DECL_NSISECUREBROWSERUI
    NS_DECL_NSISSLSTATUSPROVIDER

    void HandleDelayedDialogs();
protected:
    bool ReceiveMessage(const nsString& aMessage,
                        PRBool aSync,
                        const nsString& aJSON,
                        nsTArray<nsString>* aJSONRetVal = nsnull);

    TabParentListenerInfo* GetListenerInfo(nsIWebProgressListener *aListener);

    void ActorDestroy(ActorDestroyReason why);

    nsIDOMElement* mFrameElement;
    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;

    nsTArray<TabParentListenerInfo> mListenerInfoList;

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
    nsTArray<DelayedDialogData*> mDelayedDialogs;

    PRBool ShouldDelayDialogs();

    PRUint32 mSecurityState;
    nsString mSecurityTooltipText;
    nsCOMPtr<nsISupports> mSecurityStatusObject;

private:
    already_AddRefed<nsFrameLoader> GetFrameLoader() const;
};

} // namespace dom
} // namespace mozilla

#endif

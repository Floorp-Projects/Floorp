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

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsITabParent.h"
#include "nsIBrowserDOMWindow.h"
#include "nsWeakReference.h"
#include "nsIDialogParamBlock.h"
#include "nsIAuthPromptProvider.h"
#include "nsISecureBrowserUI.h"

class nsFrameLoader;
class nsIURI;
class nsIDOMElement;
struct gfxMatrix;

struct JSContext;
struct JSObject;

namespace mozilla {
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
protected:
    bool ReceiveMessage(const nsString& aMessage,
                        bool aSync,
                        const nsString& aJSON,
                        InfallibleTArray<nsString>* aJSONRetVal = nsnull);

    void ActorDestroy(ActorDestroyReason why);

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
    virtual PRenderFrameParent* AllocPRenderFrame();
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
    void TryCacheDPI();
};

} // namespace dom
} // namespace mozilla

#endif

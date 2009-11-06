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

#include "mozilla/dom/PIFrameEmbeddingParent.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "nsCOMPtr.h"
#include "nsIBrowserDOMWindow.h"

class nsIURI;
class nsIDOMElement;
class gfxMatrix;

struct JSContext;
struct JSObject;

namespace mozilla {

namespace jsipc {
class PContextWrapperParent;
}

namespace dom {

class TabParent : public PIFrameEmbeddingParent
{
public:
    TabParent();
    virtual ~TabParent();
    void SetOwnerElement(nsIDOMElement* aElement) { mFrameElement = aElement; }
    void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserDOMWindow) {
        mBrowserDOMWindow = aBrowserDOMWindow;
    }

    virtual bool RecvmoveFocus(const bool& aForward);
    virtual bool RecvsendEvent(const RemoteDOMEvent& aEvent);
    virtual bool AnswercreateWindow(PIFrameEmbeddingParent** retval);
    virtual bool RecvsendSyncMessageToParent(const nsString& aMessage,
                                             const nsString& aJSON,
                                             nsTArray<nsString>* aJSONRetVal);
    virtual bool RecvsendAsyncMessageToParent(const nsString& aMessage,
                                              const nsString& aJSON);

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
            const PRInt32& bufw,
            const PRInt32& bufh,
            Shmem& buf);
    virtual bool DeallocPDocumentRendererShmem(PDocumentRendererShmemParent* actor);

    virtual PContextWrapperParent* AllocPContextWrapper();
    virtual bool DeallocPContextWrapper(PContextWrapperParent* actor);

    bool GetGlobalJSObject(JSContext* cx, JSObject** globalp);

protected:
    nsIDOMElement* mFrameElement;
    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;
};

} // namespace dom
} // namespace mozilla

#endif

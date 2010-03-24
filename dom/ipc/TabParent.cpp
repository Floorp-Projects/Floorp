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

#include "TabParent.h"

#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/ipc/DocumentRendererShmemParent.h"

#include "nsIURI.h"
#include "nsFocusManager.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsEventDispatcher.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsFrameLoader.h"

using mozilla::ipc::DocumentRendererParent;
using mozilla::ipc::DocumentRendererShmemParent;

namespace mozilla {
namespace dom {

TabParent::TabParent()
{
}

TabParent::~TabParent()
{
}

bool
TabParent::RecvmoveFocus(const bool& aForward)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    nsCOMPtr<nsIDOMElement> dummy;
    PRUint32 type = aForward ? nsIFocusManager::MOVEFOCUS_FORWARD
                             : nsIFocusManager::MOVEFOCUS_BACKWARD;
    fm->MoveFocus(nsnull, mFrameElement, type, nsIFocusManager::FLAG_BYKEY, 
                  getter_AddRefs(dummy));
  }
  return true;
}

bool
TabParent::RecvsendEvent(const RemoteDOMEvent& aEvent)
{
  nsCOMPtr<nsIDOMEvent> event = do_QueryInterface(aEvent.mEvent);
  NS_ENSURE_TRUE(event, true);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mFrameElement);
  NS_ENSURE_TRUE(target, true);

  PRBool dummy;
  target->DispatchEvent(event, &dummy);
  return true;
}

bool
TabParent::AnswercreateWindow(PIFrameEmbeddingParent** retval)
{
    if (!mBrowserDOMWindow) {
        return false;
    }

    // Get a new rendering area from the browserDOMWin.  We don't want
    // to be starting any loads here, so get it with a null URI.
    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner;
    mBrowserDOMWindow->OpenURIInFrame(nsnull, nsnull,
                                      nsIBrowserDOMWindow::OPEN_NEWTAB,
                                      nsIBrowserDOMWindow::OPEN_NEW,
                                      getter_AddRefs(frameLoaderOwner));
    if (!frameLoaderOwner) {
        return false;
    }

    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (!frameLoader) {
        return false;
    }

    *retval = frameLoader->GetChildProcess();
    return true;
}

void
TabParent::LoadURL(nsIURI* aURI)
{
    nsCString spec;
    aURI->GetSpec(spec);

    SendloadURL(spec);
}

void
TabParent::Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height)
{
    Sendmove(x, y, width, height);
}

void
TabParent::Activate()
{
    Sendactivate();
}

mozilla::ipc::PDocumentRendererParent*
TabParent::AllocPDocumentRenderer(const PRInt32& x,
        const PRInt32& y, const PRInt32& w, const PRInt32& h, const nsString& bgcolor,
        const PRUint32& flags, const bool& flush)
{
    return new DocumentRendererParent();
}

bool
TabParent::DeallocPDocumentRenderer(PDocumentRendererParent* actor)
{
    delete actor;
    return true;
}

mozilla::ipc::PDocumentRendererShmemParent*
TabParent::AllocPDocumentRendererShmem(const PRInt32& x,
        const PRInt32& y, const PRInt32& w, const PRInt32& h, const nsString& bgcolor,
        const PRUint32& flags, const bool& flush, const gfxMatrix& aMatrix,
        const PRInt32& bufw, const PRInt32& bufh, Shmem &buf)
{
    return new DocumentRendererShmemParent();
}

bool
TabParent::DeallocPDocumentRendererShmem(PDocumentRendererShmemParent* actor)
{
    delete actor;
    return true;
}

void
TabParent::SendMouseEvent(const nsAString& aType, float aX, float aY,
                          PRInt32 aButton, PRInt32 aClickCount,
                          PRInt32 aModifiers, PRBool aIgnoreRootScrollFrame)
{
  SendsendMouseEvent(nsString(aType), aX, aY, aButton, aClickCount,
                     aModifiers, aIgnoreRootScrollFrame);
}

void
TabParent::SendKeyEvent(const nsAString& aType,
                        PRInt32 aKeyCode,
                        PRInt32 aCharCode,
                        PRInt32 aModifiers,
                        PRBool aPreventDefault)
{
    SendsendKeyEvent(nsString(aType), aKeyCode, aCharCode, aModifiers,
                     aPreventDefault);
}

bool
TabParent::RecvsendSyncMessageToParent(const nsString& aMessage,
                                       const nsString& aJSON,
                                       nsTArray<nsString>* aJSONRetVal)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner =
    do_QueryInterface(mFrameElement);
  if (frameLoaderOwner) {
    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (frameLoader && frameLoader->GetFrameMessageManager()) {
      frameLoader->GetFrameMessageManager()->ReceiveMessage(mFrameElement,
                                                            aMessage,
                                                            PR_TRUE,
                                                            aJSON,
                                                            aJSONRetVal);
    }
  }
  return true;
}

bool
TabParent::RecvsendAsyncMessageToParent(const nsString& aMessage,
                                        const nsString& aJSON)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner =
    do_QueryInterface(mFrameElement);
  if (frameLoaderOwner) {
    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (frameLoader && frameLoader->GetFrameMessageManager()) {
      nsTArray<nsString> dummy;
      frameLoader->GetFrameMessageManager()->ReceiveMessage(mFrameElement,
                                                            aMessage,
                                                            PR_FALSE,
                                                            aJSON,
                                                            nsnull);
    }
  }
  return true;
}

} // namespace tabs
} // namespace mozilla

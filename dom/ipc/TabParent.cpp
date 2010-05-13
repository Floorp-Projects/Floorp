/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
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
#include "mozilla/dom/ContentProcessParent.h"
#include "mozilla/jsipc/ContextWrapperParent.h"

#include "nsIURI.h"
#include "nsFocusManager.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMElement.h"
#include "nsEventDispatcher.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIWebProgressListener2.h"
#include "nsFrameLoader.h"
#include "nsNetUtil.h"
#include "jsarray.h"
#include "nsContentUtils.h"
#include "nsGeolocationOOP.h"
#include "nsIDOMNSHTMLFrameElement.h"

using mozilla::ipc::DocumentRendererParent;
using mozilla::ipc::DocumentRendererShmemParent;
using mozilla::dom::ContentProcessParent;
using mozilla::jsipc::PContextWrapperParent;
using mozilla::jsipc::ContextWrapperParent;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS1(TabParent, nsIWebProgress)

TabParent::TabParent()
{
}

TabParent::~TabParent()
{
}

void
TabParent::ActorDestroy(ActorDestroyReason why)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(mFrameElement);
  if (frameLoaderOwner) {
    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (frameLoader) {
      frameLoader->DestroyChild();
    }
  }
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
TabParent::RecvnotifyProgressChange(const PRInt64& aProgress,
                                    const PRInt64& aProgressMax,
                                    const PRInt64& aTotalProgress,
                                    const PRInt64& aMaxTotalProgress)
{
  /*
   * First notify any listeners of the new progress info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();

  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];
    if (!(info->mNotifyMask & nsIWebProgress::NOTIFY_PROGRESS)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }

    nsCOMPtr<nsIWebProgressListener2> listener2 =
      do_QueryReferent(info->mWeakListener);
    if (listener2) {
      listener2->OnProgressChange64(this, nsnull, aProgress, aProgressMax,
                                    aTotalProgress, aMaxTotalProgress);
    } else {
      listener->OnProgressChange(this, nsnull, PRInt32(aProgress),
                                 PRInt32(aProgressMax),
                                 PRInt32(aTotalProgress), 
                                 PRInt32(aMaxTotalProgress));
    }
  }

  return true;
}

bool
TabParent::RecvnotifyStateChange(const PRUint32& aStateFlags,
                                 const nsresult& aStatus)
{
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();
  
  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];

    // The flags used in listener registration are shifted over
    // 16 bits from the ones sent in the notification, so we shift
    // to see if the listener is interested in this change.
    // Note that the flags are not changed in the notification we
    // send along. Flags are defined in  nsIWebProgressListener and 
    // nsIWebProgress.
    // See nsDocLoader for another example of this.
    if (!(info->mNotifyMask & (aStateFlags >> NOTIFY_FLAG_SHIFT))) {
        continue;
    }
    
    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }
    
    listener->OnStateChange(this, nsnull, aStateFlags, aStatus); 
  }   

  return true;
 }

bool
TabParent::RecvnotifyLocationChange(const nsCString& aUri)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
  if (NS_FAILED(rv)) {
    return false;
  }

  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();

  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];
    if (!(info->mNotifyMask & nsIWebProgress::NOTIFY_LOCATION)) {
      continue;
    }
    
    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }
    
    listener->OnLocationChange(this, nsnull, uri);
  }

  return true;
}

bool
TabParent::RecvnotifyStatusChange(const nsresult& status,
                                  const nsString& message)
{
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */
  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();

  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];
    if (!(info->mNotifyMask & nsIWebProgress::NOTIFY_STATUS)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }

    listener->OnStatusChange(this, nsnull, status, message.BeginReading());
  }

  return true;
}

bool
TabParent::RecvnotifySecurityChange(const PRUint32& aState)
{
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */

  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();

  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];
    if (!(info->mNotifyMask & nsIWebProgress::NOTIFY_SECURITY)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }

    listener->OnSecurityChange(this, nsnull, aState);
  }

  return true;
}

bool
TabParent::RecvrefreshAttempted(const nsCString& aURI, const PRInt32& aMillis, 
                                const bool& aSameURI, bool* refreshAllowed)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    return false;
  }
  /*                                                                           
   * First notify any listeners of the new state info...
   *
   * Operate the elements from back to front so that if items get
   * get removed from the list it won't affect our iteration
   */

  nsCOMPtr<nsIWebProgressListener> listener;
  PRUint32 count = mListenerInfoList.Length();

  *refreshAllowed = true;
  while (count-- > 0) {
    TabParentListenerInfo *info = &mListenerInfoList[count];
    if (!(info->mNotifyMask & nsIWebProgress::NOTIFY_REFRESH)) {
      continue;
    }

    listener = do_QueryReferent(info->mWeakListener);
    if (!listener) {
      // the listener went away. gracefully pull it out of the list.
      mListenerInfoList.RemoveElementAt(count);
      continue;
    }

    nsCOMPtr<nsIWebProgressListener2> listener2 =
      do_QueryReferent(info->mWeakListener);
    if (!listener2) {
      continue;
    }

    // some listeners don't seem to set this at all...
    PRBool allowed = true;
    listener2->OnRefreshAttempted(this, uri, 
                                  aMillis, aSameURI, &allowed);
    *refreshAllowed = allowed && *refreshAllowed;
  }

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

PContextWrapperParent*
TabParent::AllocPContextWrapper()
{
    ContentProcessParent* cpp =
        static_cast<ContentProcessParent*>(Manager());
    return new ContextWrapperParent(cpp);
}

bool
TabParent::DeallocPContextWrapper(PContextWrapperParent* actor)
{
    delete actor;
    return true;
}

PGeolocationRequestParent*
TabParent::AllocPGeolocationRequest(const URI::URI& uri)
{
  return new GeolocationRequestParent(mFrameElement, uri);
}
  
bool
TabParent::DeallocPGeolocationRequest(PGeolocationRequestParent* actor)
{
  delete actor;
  return true;
}

JSBool
TabParent::GetGlobalJSObject(JSContext* cx, JSObject** globalp)
{
    // TODO Unify this code with TestShellParent::GetGlobalJSObject.
    nsTArray<PContextWrapperParent*> cwps(1);
    ManagedPContextWrapperParent(cwps);
    if (cwps.Length() < 1)
        return JS_FALSE;
    NS_ASSERTION(cwps.Length() == 1, "More than one PContextWrapper?");
    ContextWrapperParent* cwp = static_cast<ContextWrapperParent*>(cwps[0]);
    return cwp->GetGlobalJSObject(cx, globalp);
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
TabParent::AnswersendSyncMessageToParent(const nsString& aMessage,
                                         const nsString& aJSON,
                                         const nsTArray<PObjectWrapperParent*>& aObjects,
                                         nsTArray<nsString>* aJSONRetVal)
{
  static_cast<ContentProcessParent*>(Manager())->ReportChildAlreadyBlocked();
  return ReceiveMessage(aMessage, PR_TRUE, aJSON, &aObjects, aJSONRetVal);
}

bool
TabParent::RecvsendAsyncMessageToParent(const nsString& aMessage,
                                        const nsString& aJSON)
{
  return ReceiveMessage(aMessage, PR_FALSE, aJSON, nsnull);
}

bool
TabParent::ReceiveMessage(const nsString& aMessage,
                          PRBool aSync,
                          const nsString& aJSON,
                          const nsTArray<PObjectWrapperParent*>* aObjects,
                          nsTArray<nsString>* aJSONRetVal)
{
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner =
    do_QueryInterface(mFrameElement);
  if (frameLoaderOwner) {
    nsRefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
    if (frameLoader && frameLoader->GetFrameMessageManager()) {
      nsFrameMessageManager* manager = frameLoader->GetFrameMessageManager();
      JSContext* ctx = manager->GetJSContext();
      JSAutoRequest ar(ctx);
      jsval* dest;
      PRUint32 len = aObjects ? aObjects->Length() : 0;
      // Because we want JS messages to have always the same properties,
      // create array even if len == 0.
      JSObject* objectsArray =
        js_NewArrayObjectWithCapacity(ctx, len, &dest);
      if (!objectsArray) {
        return false;
      }

      nsresult rv = NS_OK;
      nsAutoGCRoot arrayGCRoot(&objectsArray, &rv);
      NS_ENSURE_SUCCESS(rv, false);
      for (PRUint32 i = 0; i < len; ++i) {
        mozilla::jsipc::ObjectWrapperParent* wrapper =
          static_cast<mozilla::jsipc::ObjectWrapperParent*>(aObjects->ElementAt(i));
        dest[i] = OBJECT_TO_JSVAL(wrapper ? wrapper->GetJSObject(ctx) : nsnull);
      }
      
      manager->ReceiveMessage(mFrameElement,
                              aMessage,
                              aSync,
                              aJSON,
                              objectsArray,
                              aJSONRetVal);
    }
  }
  return true;
}

// nsIWebProgress
nsresult
TabParent::AddProgressListener(nsIWebProgressListener* aListener,
                               PRUint32 aNotifyMask)
{
  nsresult rv;

  TabParentListenerInfo* info = GetListenerInfo(aListener);
  if (info) {
    // The listener is already registered!
    return NS_ERROR_FAILURE;
  }

  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  info = new TabParentListenerInfo(listener, aNotifyMask);
  if (!info) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mListenerInfoList.AppendElement(*info) ? NS_OK : NS_ERROR_FAILURE;
  if (NS_FAILED(rv)) {
    delete info;
  }
  return rv;
}

NS_IMETHODIMP
TabParent::RemoveProgressListener(nsIWebProgressListener *aListener)
{
  nsAutoPtr<TabParentListenerInfo> info(GetListenerInfo(aListener));
  
  return info && mListenerInfoList.RemoveElement(*info) ?
    NS_OK : NS_ERROR_FAILURE;
}

TabParentListenerInfo * 
TabParent::GetListenerInfo(nsIWebProgressListener *aListener)
{
  PRUint32 i, count;
  TabParentListenerInfo *info;

  nsCOMPtr<nsISupports> listener1 = do_QueryInterface(aListener);
  count = mListenerInfoList.Length();
  for (i = 0; i < count; ++i) {
    info = &mListenerInfoList[i];

    if (info) {
      nsCOMPtr<nsISupports> listener2 = do_QueryReferent(info->mWeakListener);
      if (listener1 == listener2) {
        return info;
      }
    }
  }
  return nsnull;
}

NS_IMETHODIMP
TabParent::GetDOMWindow(nsIDOMWindow **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabParent::GetIsLoadingDocument(PRBool *aIsLoadingDocument)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace tabs
} // namespace mozilla

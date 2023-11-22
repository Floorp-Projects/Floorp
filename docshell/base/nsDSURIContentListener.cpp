/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocShell.h"
#include "nsDSURIContentListener.h"
#include "nsIChannel.h"
#include "nsServiceManagerUtils.h"
#include "nsDocShellCID.h"
#include "nsIWebNavigationInfo.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Unused.h"
#include "nsError.h"
#include "nsContentSecurityManager.h"
#include "nsDocShellLoadTypes.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMultiPartChannel.h"
#include "nsWebNavigationInfo.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ADDREF(MaybeCloseWindowHelper)
NS_IMPL_RELEASE(MaybeCloseWindowHelper)

NS_INTERFACE_MAP_BEGIN(MaybeCloseWindowHelper)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END

MaybeCloseWindowHelper::MaybeCloseWindowHelper(BrowsingContext* aContentContext)
    : mBrowsingContext(aContentContext),
      mTimer(nullptr),
      mShouldCloseWindow(false) {}

MaybeCloseWindowHelper::~MaybeCloseWindowHelper() {}

void MaybeCloseWindowHelper::SetShouldCloseWindow(bool aShouldCloseWindow) {
  mShouldCloseWindow = aShouldCloseWindow;
}

BrowsingContext* MaybeCloseWindowHelper::MaybeCloseWindow() {
  if (!mShouldCloseWindow) {
    return mBrowsingContext;
  }

  // This method should not be called more than once, but it's better to avoid
  // closing the current window again.
  mShouldCloseWindow = false;

  // Reset the window context to the opener window so that the dependent
  // dialogs have a parent
  RefPtr<BrowsingContext> newBC = ChooseNewBrowsingContext(mBrowsingContext);

  if (newBC != mBrowsingContext && newBC && !newBC->IsDiscarded()) {
    mBCToClose = mBrowsingContext;
    mBrowsingContext = newBC;

    // Now close the old window.  Do it on a timer so that we don't run
    // into issues trying to close the window before it has fully opened.
    NS_ASSERTION(!mTimer, "mTimer was already initialized once!");
    NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, 0,
                            nsITimer::TYPE_ONE_SHOT);
  }

  return mBrowsingContext;
}

already_AddRefed<BrowsingContext>
MaybeCloseWindowHelper::ChooseNewBrowsingContext(BrowsingContext* aBC) {
  RefPtr<BrowsingContext> opener = aBC->GetOpener();
  if (opener && !opener->IsDiscarded()) {
    return opener.forget();
  }

  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  opener = BrowsingContext::Get(aBC->Canonical()->GetCrossGroupOpenerId());
  if (!opener || opener->IsDiscarded()) {
    return nullptr;
  }
  return opener.forget();
}

NS_IMETHODIMP
MaybeCloseWindowHelper::Notify(nsITimer* timer) {
  NS_ASSERTION(mBCToClose, "No window to close after timer fired");

  mBCToClose->Close(CallerType::System, IgnoreErrors());
  mBCToClose = nullptr;
  mTimer = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
MaybeCloseWindowHelper::GetName(nsACString& aName) {
  aName.AssignLiteral("MaybeCloseWindowHelper");
  return NS_OK;
}

nsDSURIContentListener::nsDSURIContentListener(nsDocShell* aDocShell)
    : mDocShell(aDocShell),
      mExistingJPEGRequest(nullptr),
      mParentContentListener(nullptr) {}

nsDSURIContentListener::~nsDSURIContentListener() {}

NS_IMPL_ADDREF(nsDSURIContentListener)
NS_IMPL_RELEASE(nsDSURIContentListener)

NS_INTERFACE_MAP_BEGIN(nsDSURIContentListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
  NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDSURIContentListener::DoContent(const nsACString& aContentType,
                                  bool aIsContentPreferred,
                                  nsIRequest* aRequest,
                                  nsIStreamListener** aContentHandler,
                                  bool* aAbortProcess) {
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aContentHandler);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  RefPtr<nsDocShell> docShell = mDocShell;

  *aAbortProcess = false;

  // determine if the channel has just been retargeted to us...
  nsLoadFlags loadFlags = 0;
  if (nsCOMPtr<nsIChannel> openedChannel = do_QueryInterface(aRequest)) {
    openedChannel->GetLoadFlags(&loadFlags);
  }

  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) {
    // XXX: Why does this not stop the content too?
    docShell->Stop(nsIWebNavigation::STOP_NETWORK);
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
    docShell->SetLoadType(aIsContentPreferred ? LOAD_LINK : LOAD_NORMAL);
  }

  // In case of multipart jpeg request (mjpeg) we don't really want to
  // create new viewer since the one we already have is capable of
  // rendering multipart jpeg correctly (see bug 625012)
  nsCOMPtr<nsIChannel> baseChannel;
  if (nsCOMPtr<nsIMultiPartChannel> mpchan = do_QueryInterface(aRequest)) {
    mpchan->GetBaseChannel(getter_AddRefs(baseChannel));
  }

  bool reuseCV = baseChannel && baseChannel == mExistingJPEGRequest &&
                 aContentType.EqualsLiteral("image/jpeg");

  if (mExistingJPEGStreamListener && reuseCV) {
    RefPtr<nsIStreamListener> copy(mExistingJPEGStreamListener);
    copy.forget(aContentHandler);
    rv = NS_OK;
  } else {
    rv =
        docShell->CreateDocumentViewer(aContentType, aRequest, aContentHandler);
    if (NS_SUCCEEDED(rv) && reuseCV) {
      mExistingJPEGStreamListener = *aContentHandler;
    } else {
      mExistingJPEGStreamListener = nullptr;
    }
    mExistingJPEGRequest = baseChannel;
  }

  if (rv == NS_ERROR_DOCSHELL_DYING) {
    aRequest->Cancel(rv);
    *aAbortProcess = true;
    return NS_OK;
  }

  if (NS_FAILED(rv)) {
    // we don't know how to handle the content
    nsCOMPtr<nsIStreamListener> forget = dont_AddRef(*aContentHandler);
    *aContentHandler = nullptr;
    return rv;
  }

  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) {
    nsCOMPtr<nsPIDOMWindowOuter> domWindow =
        mDocShell ? mDocShell->GetWindow() : nullptr;
    NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);
    domWindow->Focus(mozilla::dom::CallerType::System);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::IsPreferred(const char* aContentType,
                                    char** aDesiredContentType,
                                    bool* aCanHandle) {
  NS_ENSURE_ARG_POINTER(aCanHandle);
  NS_ENSURE_ARG_POINTER(aDesiredContentType);

  // the docshell has no idea if it is the preferred content provider or not.
  // It needs to ask its parent if it is the preferred content handler or not...

  nsCOMPtr<nsIURIContentListener> parentListener;
  GetParentContentListener(getter_AddRefs(parentListener));
  if (parentListener) {
    return parentListener->IsPreferred(aContentType, aDesiredContentType,
                                       aCanHandle);
  }
  // we used to return false here if we didn't have a parent properly registered
  // at the top of the docshell hierarchy to dictate what content types this
  // docshell should be a preferred handler for. But this really makes it hard
  // for developers using iframe or browser tags because then they need to make
  // sure they implement nsIURIContentListener otherwise all link clicks would
  // get sent to another window because we said we weren't the preferred handler
  // type. I'm going to change the default now... if we can handle the content,
  // and someone didn't EXPLICITLY set a nsIURIContentListener at the top of our
  // docshell chain, then we'll now always attempt to process the content
  // ourselves...
  return CanHandleContent(aContentType, true, aDesiredContentType, aCanHandle);
}

NS_IMETHODIMP
nsDSURIContentListener::CanHandleContent(const char* aContentType,
                                         bool aIsContentPreferred,
                                         char** aDesiredContentType,
                                         bool* aCanHandleContent) {
  MOZ_ASSERT(aCanHandleContent, "Null out param?");
  NS_ENSURE_ARG_POINTER(aDesiredContentType);

  *aCanHandleContent = false;
  *aDesiredContentType = nullptr;

  if (aContentType) {
    uint32_t canHandle =
        nsWebNavigationInfo::IsTypeSupported(nsDependentCString(aContentType));
    *aCanHandleContent = (canHandle != nsIWebNavigationInfo::UNSUPPORTED);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::GetLoadCookie(nsISupports** aLoadCookie) {
  NS_IF_ADDREF(*aLoadCookie = nsDocShell::GetAsSupports(mDocShell));
  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetLoadCookie(nsISupports* aLoadCookie) {
#ifdef DEBUG
  RefPtr<nsDocLoader> cookieAsDocLoader =
      nsDocLoader::GetAsDocLoader(aLoadCookie);
  NS_ASSERTION(cookieAsDocLoader && cookieAsDocLoader == mDocShell,
               "Invalid load cookie being set!");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::GetParentContentListener(
    nsIURIContentListener** aParentListener) {
  if (mWeakParentContentListener) {
    nsCOMPtr<nsIURIContentListener> tempListener =
        do_QueryReferent(mWeakParentContentListener);
    *aParentListener = tempListener;
    NS_IF_ADDREF(*aParentListener);
  } else {
    *aParentListener = mParentContentListener;
    NS_IF_ADDREF(*aParentListener);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetParentContentListener(
    nsIURIContentListener* aParentListener) {
  if (aParentListener) {
    // Store the parent listener as a weak ref. Parents not supporting
    // nsISupportsWeakReference assert but may still be used.
    mParentContentListener = nullptr;
    mWeakParentContentListener = do_GetWeakReference(aParentListener);
    if (!mWeakParentContentListener) {
      mParentContentListener = aParentListener;
    }
  } else {
    mWeakParentContentListener = nullptr;
    mParentContentListener = nullptr;
  }
  return NS_OK;
}

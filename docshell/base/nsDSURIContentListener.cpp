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
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsError.h"
#include "nsContentSecurityManager.h"
#include "nsDocShellLoadTypes.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMultiPartChannel.h"

using namespace mozilla;

NS_IMPL_ADDREF(MaybeCloseWindowHelper)
NS_IMPL_RELEASE(MaybeCloseWindowHelper)

NS_INTERFACE_MAP_BEGIN(MaybeCloseWindowHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MaybeCloseWindowHelper::MaybeCloseWindowHelper(nsIInterfaceRequestor* aContentContext)
  : mContentContext(aContentContext)
  , mWindowToClose(nullptr)
  , mTimer(nullptr)
  , mShouldCloseWindow(false)
{
}

MaybeCloseWindowHelper::~MaybeCloseWindowHelper()
{
}

void
MaybeCloseWindowHelper::SetShouldCloseWindow(bool aShouldCloseWindow)
{
  mShouldCloseWindow = aShouldCloseWindow;
}

nsIInterfaceRequestor*
MaybeCloseWindowHelper::MaybeCloseWindow()
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mContentContext);
  NS_ENSURE_TRUE(window, mContentContext);

  if (mShouldCloseWindow) {
    // Reset the window context to the opener window so that the dependent
    // dialogs have a parent
    nsCOMPtr<nsPIDOMWindowOuter> opener = window->GetOpener();

    if (opener && !opener->Closed()) {
      mContentContext = do_GetInterface(opener);

      // Now close the old window.  Do it on a timer so that we don't run
      // into issues trying to close the window before it has fully opened.
      NS_ASSERTION(!mTimer, "mTimer was already initialized once!");
      NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, 0, nsITimer::TYPE_ONE_SHOT);
      mWindowToClose = window;
    }
  }
  return mContentContext;
}

NS_IMETHODIMP
MaybeCloseWindowHelper::Notify(nsITimer* timer)
{
  NS_ASSERTION(mWindowToClose, "No window to close after timer fired");

  mWindowToClose->Close();
  mWindowToClose = nullptr;
  mTimer = nullptr;

  return NS_OK;
}

nsDSURIContentListener::nsDSURIContentListener(nsDocShell* aDocShell)
  : mDocShell(aDocShell)
  , mExistingJPEGRequest(nullptr)
  , mParentContentListener(nullptr)
{
}

nsDSURIContentListener::~nsDSURIContentListener()
{
}

nsresult
nsDSURIContentListener::Init()
{
  nsresult rv;
  mNavInfo = do_GetService(NS_WEBNAVIGATION_INFO_CONTRACTID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get webnav info");
  return rv;
}

NS_IMPL_ADDREF(nsDSURIContentListener)
NS_IMPL_RELEASE(nsDSURIContentListener)

NS_INTERFACE_MAP_BEGIN(nsDSURIContentListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
  NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDSURIContentListener::OnStartURIOpen(nsIURI* aURI, bool* aAbortOpen)
{
  // If mDocShell is null here, that means someone's starting a load in our
  // docshell after it's already been destroyed.  Don't let that happen.
  if (!mDocShell) {
    *aAbortOpen = true;
    return NS_OK;
  }

  nsCOMPtr<nsIURIContentListener> parentListener;
  GetParentContentListener(getter_AddRefs(parentListener));
  if (parentListener) {
    return parentListener->OnStartURIOpen(aURI, aAbortOpen);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::DoContent(const nsACString& aContentType,
                                  bool aIsContentPreferred,
                                  nsIRequest* aRequest,
                                  nsIStreamListener** aContentHandler,
                                  bool* aAbortProcess)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aContentHandler);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  *aAbortProcess = false;

  // determine if the channel has just been retargeted to us...
  nsLoadFlags loadFlags = 0;
  nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(aRequest);

  if (aOpenedChannel) {
    aOpenedChannel->GetLoadFlags(&loadFlags);

    // block top-level data URI navigations if triggered by the web
    if (!nsContentSecurityManager::AllowTopLevelNavigationToDataURI(aOpenedChannel)) {
      // logging to console happens within AllowTopLevelNavigationToDataURI
      aRequest->Cancel(NS_ERROR_DOM_BAD_URI);
      *aAbortProcess = true;
      // close the window since the navigation to a data URI was blocked
      if (mDocShell) {
        nsCOMPtr<nsIInterfaceRequestor> contentContext =
          do_QueryInterface(mDocShell->GetWindow());
        if (contentContext) {
          RefPtr<MaybeCloseWindowHelper> maybeCloseWindowHelper =
            new MaybeCloseWindowHelper(contentContext);
          maybeCloseWindowHelper->SetShouldCloseWindow(true);
          maybeCloseWindowHelper->MaybeCloseWindow();
        }
      }
      return NS_OK; 
    }
  }

  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) {
    // XXX: Why does this not stop the content too?
    mDocShell->Stop(nsIWebNavigation::STOP_NETWORK);

    mDocShell->SetLoadType(aIsContentPreferred ? LOAD_LINK : LOAD_NORMAL);
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
    rv = mDocShell->CreateContentViewer(aContentType, aRequest, aContentHandler);
    if (NS_SUCCEEDED(rv) && reuseCV) {
      mExistingJPEGStreamListener = *aContentHandler;
    } else {
      mExistingJPEGStreamListener = nullptr;
    }
    mExistingJPEGRequest = baseChannel;
  }

  if (rv == NS_ERROR_REMOTE_XUL || rv == NS_ERROR_DOCSHELL_DYING) {
    aRequest->Cancel(rv);
    *aAbortProcess = true;
    return NS_OK;
  }

  if (NS_FAILED(rv)) {
    // we don't know how to handle the content
    *aContentHandler = nullptr;
    return rv;
  }

  if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) {
    nsCOMPtr<nsPIDOMWindowOuter> domWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
    NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);
    domWindow->Focus();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::IsPreferred(const char* aContentType,
                                    char** aDesiredContentType,
                                    bool* aCanHandle)
{
  NS_ENSURE_ARG_POINTER(aCanHandle);
  NS_ENSURE_ARG_POINTER(aDesiredContentType);

  // the docshell has no idea if it is the preferred content provider or not.
  // It needs to ask its parent if it is the preferred content handler or not...

  nsCOMPtr<nsIURIContentListener> parentListener;
  GetParentContentListener(getter_AddRefs(parentListener));
  if (parentListener) {
    return parentListener->IsPreferred(aContentType,
                                       aDesiredContentType,
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
                                         bool* aCanHandleContent)
{
  MOZ_ASSERT(aCanHandleContent, "Null out param?");
  NS_ENSURE_ARG_POINTER(aDesiredContentType);

  *aCanHandleContent = false;
  *aDesiredContentType = nullptr;

  nsresult rv = NS_OK;
  if (aContentType) {
    uint32_t canHandle = nsIWebNavigationInfo::UNSUPPORTED;
    rv = mNavInfo->IsTypeSupported(nsDependentCString(aContentType),
                                   mDocShell,
                                   &canHandle);
    *aCanHandleContent = (canHandle != nsIWebNavigationInfo::UNSUPPORTED);
  }

  return rv;
}

NS_IMETHODIMP
nsDSURIContentListener::GetLoadCookie(nsISupports** aLoadCookie)
{
  NS_IF_ADDREF(*aLoadCookie = nsDocShell::GetAsSupports(mDocShell));
  return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetLoadCookie(nsISupports* aLoadCookie)
{
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
    nsIURIContentListener** aParentListener)
{
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
    nsIURIContentListener* aParentListener)
{
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

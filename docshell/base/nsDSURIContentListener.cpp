/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsIHttpChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsError.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsDocShellLoadTypes.h"

using namespace mozilla;

//*****************************************************************************
//***    nsDSURIContentListener: Object Management
//*****************************************************************************

nsDSURIContentListener::nsDSURIContentListener(nsDocShell* aDocShell)
    : mDocShell(aDocShell), 
      mParentContentListener(nullptr)
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


//*****************************************************************************
// nsDSURIContentListener::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDSURIContentListener)
NS_IMPL_RELEASE(nsDSURIContentListener)

NS_INTERFACE_MAP_BEGIN(nsDSURIContentListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
    NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDSURIContentListener::nsIURIContentListener
//*****************************************************************************   

NS_IMETHODIMP
nsDSURIContentListener::OnStartURIOpen(nsIURI* aURI, bool* aAbortOpen)
{
    // If mDocShell is null here, that means someone's starting a load
    // in our docshell after it's already been destroyed.  Don't let
    // that happen.
    if (!mDocShell) {
        *aAbortOpen = true;
        return NS_OK;
    }
    
    nsCOMPtr<nsIURIContentListener> parentListener;
    GetParentContentListener(getter_AddRefs(parentListener));
    if (parentListener)
        return parentListener->OnStartURIOpen(aURI, aAbortOpen);

    return NS_OK;
}

NS_IMETHODIMP 
nsDSURIContentListener::DoContent(const char* aContentType, 
                                  bool aIsContentPreferred,
                                  nsIRequest* request,
                                  nsIStreamListener** aContentHandler,
                                  bool* aAbortProcess)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aContentHandler);
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

    // Check whether X-Frame-Options permits us to load this content in an
    // iframe and abort the load (unless we've disabled x-frame-options
    // checking).
    if (!CheckFrameOptions(request)) {
        *aAbortProcess = true;
        return NS_OK;
    }

    *aAbortProcess = false;

    // determine if the channel has just been retargeted to us...
    nsLoadFlags loadFlags = 0;
    nsCOMPtr<nsIChannel> aOpenedChannel = do_QueryInterface(request);

    if (aOpenedChannel)
      aOpenedChannel->GetLoadFlags(&loadFlags);

    if(loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
    {
        // XXX: Why does this not stop the content too?
        mDocShell->Stop(nsIWebNavigation::STOP_NETWORK);

        mDocShell->SetLoadType(aIsContentPreferred ? LOAD_LINK : LOAD_NORMAL);
    }

    rv = mDocShell->CreateContentViewer(aContentType, request, aContentHandler);

    if (rv == NS_ERROR_REMOTE_XUL) {
      request->Cancel(rv);
      *aAbortProcess = true;
      return NS_OK;
    }

    if (NS_FAILED(rv)) { 
      // we don't know how to handle the content
      *aContentHandler = nullptr;
      return rv;
    }

    if (loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI) {
        nsCOMPtr<nsIDOMWindow> domWindow = do_GetInterface(static_cast<nsIDocShell*>(mDocShell));
        NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);
        domWindow->Focus();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::IsPreferred(const char* aContentType,
                                    char ** aDesiredContentType,
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
    // we used to return false here if we didn't have a parent properly
    // registered at the top of the docshell hierarchy to dictate what
    // content types this docshell should be a preferred handler for.  But
    // this really makes it hard for developers using iframe or browser tags
    // because then they need to make sure they implement
    // nsIURIContentListener otherwise all link clicks would get sent to
    // another window because we said we weren't the preferred handler type.
    // I'm going to change the default now...if we can handle the content,
    // and someone didn't EXPLICITLY set a nsIURIContentListener at the top
    // of our docshell chain, then we'll now always attempt to process the
    // content ourselves...
    return CanHandleContent(aContentType,
                            true,
                            aDesiredContentType,
                            aCanHandle);
}

NS_IMETHODIMP
nsDSURIContentListener::CanHandleContent(const char* aContentType,
                                         bool aIsContentPreferred,
                                         char ** aDesiredContentType,
                                         bool* aCanHandleContent)
{
    NS_PRECONDITION(aCanHandleContent, "Null out param?");
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
nsDSURIContentListener::GetLoadCookie(nsISupports ** aLoadCookie)
{
    NS_IF_ADDREF(*aLoadCookie = nsDocShell::GetAsSupports(mDocShell));
    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
#ifdef DEBUG
    nsRefPtr<nsDocLoader> cookieAsDocLoader =
        nsDocLoader::GetAsDocLoader(aLoadCookie);
    NS_ASSERTION(cookieAsDocLoader && cookieAsDocLoader == mDocShell,
                 "Invalid load cookie being set!");
#endif
    return NS_OK;
}

NS_IMETHODIMP 
nsDSURIContentListener::GetParentContentListener(nsIURIContentListener**
                                                 aParentListener)
{
    if (mWeakParentContentListener)
    {
        nsCOMPtr<nsIURIContentListener> tempListener =
            do_QueryReferent(mWeakParentContentListener);
        *aParentListener = tempListener;
        NS_IF_ADDREF(*aParentListener);
    }
    else {
        *aParentListener = mParentContentListener;
        NS_IF_ADDREF(*aParentListener);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetParentContentListener(nsIURIContentListener* 
                                                 aParentListener)
{
    if (aParentListener)
    {
        // Store the parent listener as a weak ref. Parents not supporting
        // nsISupportsWeakReference assert but may still be used.
        mParentContentListener = nullptr;
        mWeakParentContentListener = do_GetWeakReference(aParentListener);
        if (!mWeakParentContentListener)
        {
            mParentContentListener = aParentListener;
        }
    }
    else
    {
        mWeakParentContentListener = nullptr;
        mParentContentListener = nullptr;
    }
    return NS_OK;
}

bool nsDSURIContentListener::CheckOneFrameOptionsPolicy(nsIHttpChannel *httpChannel,
                                                        const nsAString& policy) {
    static const char allowFrom[] = "allow-from";
    const uint32_t allowFromLen = ArrayLength(allowFrom) - 1;
    bool isAllowFrom =
        StringHead(policy, allowFromLen).LowerCaseEqualsLiteral(allowFrom);

    // return early if header does not have one of the values with meaning
    if (!policy.LowerCaseEqualsLiteral("deny") &&
        !policy.LowerCaseEqualsLiteral("sameorigin") &&
        !isAllowFrom)
        return true;

    nsCOMPtr<nsIURI> uri;
    httpChannel->GetURI(getter_AddRefs(uri));

    // XXXkhuey when does this happen?  Is returning true safe here?
    if (!mDocShell) {
        return true;
    }

    // We need to check the location of this window and the location of the top
    // window, if we're not the top.  X-F-O: SAMEORIGIN requires that the
    // document must be same-origin with top window.  X-F-O: DENY requires that
    // the document must never be framed.
    nsCOMPtr<nsIDOMWindow> thisWindow = do_GetInterface(static_cast<nsIDocShell*>(mDocShell));
    // If we don't have DOMWindow there is no risk of clickjacking
    if (!thisWindow)
        return true;

    // GetScriptableTop, not GetTop, because we want this to respect
    // <iframe mozbrowser> boundaries.
    nsCOMPtr<nsIDOMWindow> topWindow;
    thisWindow->GetScriptableTop(getter_AddRefs(topWindow));

    // if the document is in the top window, it's not in a frame.
    if (thisWindow == topWindow)
        return true;

    // Find the top docshell in our parent chain that doesn't have the system
    // principal and use it for the principal comparison.  Finding the top
    // content-type docshell doesn't work because some chrome documents are
    // loaded in content docshells (see bug 593387).
    nsCOMPtr<nsIDocShellTreeItem> thisDocShellItem(do_QueryInterface(
                                                   static_cast<nsIDocShell*> (mDocShell)));
    nsCOMPtr<nsIDocShellTreeItem> parentDocShellItem,
                                  curDocShellItem = thisDocShellItem;
    nsCOMPtr<nsIDocument> topDoc;
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (!ssm) {
        MOZ_CRASH();
    }

    // Traverse up the parent chain and stop when we see a docshell whose
    // parent has a system principal, or a docshell corresponding to
    // <iframe mozbrowser>.
    while (NS_SUCCEEDED(curDocShellItem->GetParent(getter_AddRefs(parentDocShellItem))) &&
           parentDocShellItem) {

        nsCOMPtr<nsIDocShell> curDocShell = do_QueryInterface(curDocShellItem);
        if (curDocShell && curDocShell->GetIsBrowserOrApp()) {
          break;
        }

        bool system = false;
        topDoc = do_GetInterface(parentDocShellItem);
        if (topDoc) {
            if (NS_SUCCEEDED(ssm->IsSystemPrincipal(topDoc->NodePrincipal(),
                                                    &system)) && system) {
                // Found a system-principled doc: last docshell was top.
                break;
            }
        }
        else {
            return false;
        }
        curDocShellItem = parentDocShellItem;
    }

    // If this document has the top non-SystemPrincipal docshell it is not being
    // framed or it is being framed by a chrome document, which we allow.
    if (curDocShellItem == thisDocShellItem)
        return true;

    // If the value of the header is DENY, and the previous condition is
    // not met (current docshell is not the top docshell), prohibit the
    // load.
    if (policy.LowerCaseEqualsLiteral("deny")) {
        ReportXFOViolation(curDocShellItem, uri, eDENY);
        return false;
    }

    topDoc = do_GetInterface(curDocShellItem);
    nsCOMPtr<nsIURI> topUri;
    topDoc->NodePrincipal()->GetURI(getter_AddRefs(topUri));

    // If the X-Frame-Options value is SAMEORIGIN, then the top frame in the
    // parent chain must be from the same origin as this document.
    if (policy.LowerCaseEqualsLiteral("sameorigin")) {
        rv = ssm->CheckSameOriginURI(uri, topUri, true);
        if (NS_FAILED(rv)) {
            ReportXFOViolation(curDocShellItem, uri, eSAMEORIGIN);
            return false; /* wasn't same-origin */
        }
    }

    // If the X-Frame-Options value is "allow-from [uri]", then the top
    // frame in the parent chain must be from that origin
    if (isAllowFrom) {
        if (policy.Length() == allowFromLen ||
            (policy[allowFromLen] != ' ' &&
             policy[allowFromLen] != '\t')) {
            ReportXFOViolation(curDocShellItem, uri, eALLOWFROM);
            return false;
        }
        rv = NS_NewURI(getter_AddRefs(uri),
                       Substring(policy, allowFromLen));
        if (NS_FAILED(rv))
          return false;

        rv = ssm->CheckSameOriginURI(uri, topUri, true);
        if (NS_FAILED(rv)) {
            ReportXFOViolation(curDocShellItem, uri, eALLOWFROM);
            return false;
        }
    }

    return true;
}

// Check if X-Frame-Options permits this document to be loaded as a subdocument.
// This will iterate through and check any number of X-Frame-Options policies
// in the request (comma-separated in a header, multiple headers, etc).
bool nsDSURIContentListener::CheckFrameOptions(nsIRequest *request)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> chan = do_QueryInterface(request);
    if (!chan) {
      return true;
    }

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(chan);
    if (!httpChannel) {
      // check if it is hiding in a multipart channel
      rv = mDocShell->GetHttpChannel(chan, getter_AddRefs(httpChannel));
      if (NS_FAILED(rv))
        return false;
    }

    if (!httpChannel) {
        return true;
    }

    nsAutoCString xfoHeaderCValue;
    httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-Frame-Options"),
                                   xfoHeaderCValue);
    NS_ConvertUTF8toUTF16 xfoHeaderValue(xfoHeaderCValue);

    // if no header value, there's nothing to do.
    if (xfoHeaderValue.IsEmpty())
        return true;

    // iterate through all the header values (usually there's only one, but can
    // be many.  If any want to deny the load, deny the load.
    nsCharSeparatedTokenizer tokenizer(xfoHeaderValue, ',');
    while (tokenizer.hasMoreTokens()) {
        const nsSubstring& tok = tokenizer.nextToken();
        if (!CheckOneFrameOptionsPolicy(httpChannel, tok)) {
            // cancel the load and display about:blank
            httpChannel->Cancel(NS_BINDING_ABORTED);
            if (mDocShell) {
                nsCOMPtr<nsIWebNavigation> webNav(do_QueryObject(mDocShell));
                if (webNav) {
                    webNav->LoadURI(NS_LITERAL_STRING("about:blank").get(),
                                    0, nullptr, nullptr, nullptr);
                }
            }
            return false;
        }
    }

    return true;
}

void
nsDSURIContentListener::ReportXFOViolation(nsIDocShellTreeItem* aTopDocShellItem,
                                           nsIURI* aThisURI,
                                           XFOHeader aHeader)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsPIDOMWindow> topOuterWindow = do_GetInterface(aTopDocShellItem);
    if (!topOuterWindow)
        return;

    NS_ASSERTION(topOuterWindow->IsOuterWindow(), "Huh?");
    nsPIDOMWindow* topInnerWindow = topOuterWindow->GetCurrentInnerWindow();
    if (!topInnerWindow)
        return;

    nsCOMPtr<nsIURI> topURI;

    nsCOMPtr<nsIDocument> document;

    document = do_GetInterface(aTopDocShellItem);
    rv = document->NodePrincipal()->GetURI(getter_AddRefs(topURI));
    if (NS_FAILED(rv))
        return;

    if (!topURI)
        return;

    nsCString topURIString;
    nsCString thisURIString;

    rv = topURI->GetSpec(topURIString);
    if (NS_FAILED(rv))
        return;

    rv = aThisURI->GetSpec(thisURIString);
    if (NS_FAILED(rv))
        return;

    nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

    if (!consoleService || !errorObject)
        return;

    nsString msg = NS_LITERAL_STRING("Load denied by X-Frame-Options: ");
    msg.Append(NS_ConvertUTF8toUTF16(thisURIString));

    switch (aHeader) {
        case eDENY:
            msg.AppendLiteral(" does not permit framing.");
            break;
        case eSAMEORIGIN:
            msg.AppendLiteral(" does not permit cross-origin framing.");
            break;
        case eALLOWFROM:
            msg.AppendLiteral(" does not permit framing by ");
            msg.Append(NS_ConvertUTF8toUTF16(topURIString));
            msg.AppendLiteral(".");
            break;
    }

    rv = errorObject->InitWithWindowID(msg, EmptyString(), EmptyString(), 0, 0,
                                       nsIScriptError::errorFlag,
                                       "X-Frame-Options",
                                       topInnerWindow->WindowID());
    if (NS_FAILED(rv))
        return;

    consoleService->LogMessage(errorObject);
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDocShell.h"
#include "nsDSURIContentListener.h"
#include "nsIChannel.h"
#include "nsServiceManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsDocShellCID.h"
#include "nsIWebNavigationInfo.h"
#include "nsIDOMWindow.h"
#include "nsAutoPtr.h"
#include "nsIHttpChannel.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetError.h"

//*****************************************************************************
//***    nsDSURIContentListener: Object Management
//*****************************************************************************

nsDSURIContentListener::nsDSURIContentListener(nsDocShell* aDocShell)
    : mDocShell(aDocShell), 
      mParentContentListener(nsnull)
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

NS_IMPL_THREADSAFE_ADDREF(nsDSURIContentListener)
NS_IMPL_THREADSAFE_RELEASE(nsDSURIContentListener)

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
        *aAbortOpen = PR_TRUE;
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
    // iframe
    if (!CheckFrameOptions(request)) {
        *aAbortProcess = PR_TRUE;
        return NS_OK;
    }

    *aAbortProcess = PR_FALSE;

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
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
       // it's okay if we don't know how to handle the content   
        return NS_OK;
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
                            PR_TRUE,
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

    *aCanHandleContent = PR_FALSE;
    *aDesiredContentType = nsnull;

    nsresult rv = NS_OK;
    if (aContentType) {
        PRUint32 canHandle = nsIWebNavigationInfo::UNSUPPORTED;
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
        mParentContentListener = nsnull;
        mWeakParentContentListener = do_GetWeakReference(aParentListener);
        if (!mWeakParentContentListener)
        {
            mParentContentListener = aParentListener;
        }
    }
    else
    {
        mWeakParentContentListener = nsnull;
        mParentContentListener = nsnull;
    }
    return NS_OK;
}

// Check if X-Frame-Options permits this document to be loaded as a subdocument.
bool nsDSURIContentListener::CheckFrameOptions(nsIRequest* request)
{
    nsCAutoString xfoHeaderValue;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
    if (!httpChannel) {
        return true;
    }

    httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-Frame-Options"),
                                   xfoHeaderValue);

    // return early if header does not have one of the two values with meaning
    if (!xfoHeaderValue.LowerCaseEqualsLiteral("deny") &&
        !xfoHeaderValue.LowerCaseEqualsLiteral("sameorigin"))
        return true;

    if (mDocShell) {
        // We need to check the location of this window and the location of the top
        // window, if we're not the top.  X-F-O: SAMEORIGIN requires that the
        // document must be same-origin with top window.  X-F-O: DENY requires that
        // the document must never be framed.
        nsCOMPtr<nsIDOMWindow> thisWindow = do_GetInterface(static_cast<nsIDocShell*>(mDocShell));
        // If we don't have DOMWindow there is no risk of clickjacking
        if (!thisWindow)
            return true;

        nsCOMPtr<nsIDOMWindow> topWindow;
        thisWindow->GetTop(getter_AddRefs(topWindow));

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
        if (!ssm)
            return false;

        // Traverse up the parent chain to the top docshell that doesn't have
        // a system principal
        while (NS_SUCCEEDED(curDocShellItem->GetParent(getter_AddRefs(parentDocShellItem))) &&
               parentDocShellItem) {
            bool system = false;
            topDoc = do_GetInterface(parentDocShellItem);
            if (topDoc) {
                if (NS_SUCCEEDED(ssm->IsSystemPrincipal(topDoc->NodePrincipal(),
                                                        &system)) && system) {
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

        // If the X-Frame-Options value is SAMEORIGIN, then the top frame in the
        // parent chain must be from the same origin as this document.
        if (xfoHeaderValue.LowerCaseEqualsLiteral("sameorigin")) {
            nsCOMPtr<nsIURI> uri;
            httpChannel->GetURI(getter_AddRefs(uri));
            topDoc = do_GetInterface(curDocShellItem);
            nsCOMPtr<nsIURI> topUri;
            topDoc->NodePrincipal()->GetURI(getter_AddRefs(topUri));
            rv = ssm->CheckSameOriginURI(uri, topUri, PR_TRUE);
            if (NS_SUCCEEDED(rv))
                return true;
        }

        else {
            // If the value of the header is DENY, then the document
            // should never be permitted to load as a subdocument.
            NS_ASSERTION(xfoHeaderValue.LowerCaseEqualsLiteral("deny"),
                         "How did we get here with some random header value?");
        }

        // cancel the load and display about:blank
        httpChannel->Cancel(NS_BINDING_ABORTED);
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryObject(mDocShell));
        if (webNav) {
            webNav->LoadURI(NS_LITERAL_STRING("about:blank").get(),
                            0, nsnull, nsnull, nsnull);
        }
        return false;
    }

    return true;
}

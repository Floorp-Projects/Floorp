/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsDocShell.h"
#include "nsDSURIContentListener.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"

//*****************************************************************************
//***    nsDSURIContentListener: Object Management
//*****************************************************************************

nsDSURIContentListener::nsDSURIContentListener() : mDocShell(nsnull), 
    mParentContentListener(nsnull)
{
    NS_INIT_REFCNT();
}

nsDSURIContentListener::~nsDSURIContentListener()
{
}

nsresult
nsDSURIContentListener::Init() 
{
    nsresult rv = NS_OK;
    mCatMgr = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
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
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsDSURIContentListener::nsIURIContentListener
//*****************************************************************************   

NS_IMETHODIMP
nsDSURIContentListener::OnStartURIOpen(nsIURI* aURI, PRBool* aAbortOpen)
{
    if(mParentContentListener)
        return mParentContentListener->OnStartURIOpen(aURI, aAbortOpen);

    return NS_OK;
}

NS_IMETHODIMP 
nsDSURIContentListener::DoContent(const char* aContentType, 
                                  PRBool aIsContentPreferred,
                                  nsIRequest* request,
                                  nsIStreamListener** aContentHandler,
                                  PRBool* aAbortProcess)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aContentHandler);
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
    if(aAbortProcess)
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
    if (NS_FAILED(rv)) {
       // it's okay if we don't know how to handle the content   
        return NS_OK;
    }

    if(loadFlags & nsIChannel::LOAD_RETARGETED_DOCUMENT_URI)
        mDocShell->SetFocus();

    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::IsPreferred(const char* aContentType,
                                    char ** aDesiredContentType,
                                    PRBool* aCanHandle)
{
    NS_ENSURE_ARG_POINTER(aCanHandle);
    NS_ENSURE_ARG_POINTER(aDesiredContentType);

    // the docshell has no idea if it is the preferred content provider or not.
    // It needs to ask it's parent if it is the preferred content handler or not...

    if(mParentContentListener) {
        return mParentContentListener->IsPreferred(aContentType,
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
                                         PRBool aIsContentPreferred,
                                         char ** aDesiredContentType,
                                         PRBool* aCanHandleContent)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aCanHandleContent);
    NS_ENSURE_ARG_POINTER(aDesiredContentType);

    *aCanHandleContent = PR_FALSE;

    if (aContentType)
    {
        nsXPIDLCString value;
        rv = mCatMgr->GetCategoryEntry("Gecko-Content-Viewers",
                                       aContentType, 
                                       getter_Copies(value));

        // If the category manager can't find what we're looking for
        // it returns NS_ERROR_NOT_AVAILABLE, we don't wanto propagate
        // that to the caller since it's really not a failure

        if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE)
            return rv;

        if (value && *value)
            *aCanHandleContent = PR_TRUE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::GetLoadCookie(nsISupports ** aLoadCookie)
{
    return mDocShell->GetLoadCookie(aLoadCookie);
}

NS_IMETHODIMP
nsDSURIContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
    return mDocShell->SetLoadCookie(aLoadCookie);
}

NS_IMETHODIMP 
nsDSURIContentListener::GetParentContentListener(nsIURIContentListener**
                                                 aParentListener)
{
    *aParentListener = mParentContentListener;
    NS_IF_ADDREF(*aParentListener);
    return NS_OK;
}

NS_IMETHODIMP
nsDSURIContentListener::SetParentContentListener(nsIURIContentListener* 
                                                 aParentListener)
{
    // Weak Reference, don't addref
    mParentContentListener = aParentListener;
    return NS_OK;
}

//*****************************************************************************
// nsDSURIContentListener: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDSURIContentListener: Accessors
//*****************************************************************************   

void nsDSURIContentListener::DocShell(nsDocShell* aDocShell)
{
    mDocShell = aDocShell;
}

nsDocShell* nsDSURIContentListener::DocShell()
{
    return mDocShell;
}

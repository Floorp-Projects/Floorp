/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Ed Burns <edburns@acm.org>
 */

#include "AjaxListener.h"
#include "EmbedProgress.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsIXMLHTTPRequest.h"
#include "nsIInterfaceRequestor.h"

#include "jni_util.h"

NS_IMPL_ISUPPORTS1(AjaxListener, nsIObserver)

AjaxListener::AjaxListener(EmbedProgress *owner, 
			   JNIEnv *env,
			   jobject eventRegistration) : 
    mOwner(owner),
    mJNIEnv(env),
    mEventRegistration(eventRegistration)
{
}

AjaxListener::~AjaxListener()
{
}

//
// Methods from nsIObserver
// 

NS_IMETHODIMP 
AjaxListener::Observe(nsISupports *aSubject, 
		      const char *aTopic, const PRUnichar *aData)
{
    nsresult rv = NS_OK;
    if (0 != strcmp("http-on-modify-request", aTopic)) {
	return rv;
    }

    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aSubject, &rv);
    nsCOMPtr<nsIRequest> request = nsnull;
    nsCOMPtr<nsIInterfaceRequestor> iface = nsnull;
    nsCOMPtr<nsIXMLHttpRequest> ajax = nsnull;
    nsLoadFlags loadFlags = 0;

    if (NS_SUCCEEDED(rv) && channel) {
	request = do_QueryInterface(aSubject, &rv);
	if (NS_SUCCEEDED(rv) && request) {
	    rv = request->GetLoadFlags(&loadFlags);
	    // If this is an XMLHttpRequest,loadFlags would
	    // have LOAD_BACKGROUND
	    if (NS_SUCCEEDED(rv) &&
		(loadFlags & nsIRequest::LOAD_BACKGROUND)) {
		rv = channel->GetNotificationCallbacks(getter_AddRefs(iface));
		if (NS_SUCCEEDED(rv) && iface) {
		    ajax = do_QueryInterface(iface, &rv);
		    if (NS_SUCCEEDED(rv) && ajax) {
			loadFlags = 0;
		    }
		    else {
			// It's ok if we don't have an xmlhttprequest
			rv = NS_OK;
		    }
		}
		else {
		    // It's ok if we don't have an iface.
		    rv = NS_OK;
		}
	    }
	}
    }

    return rv;
}

//
// Webclient private methods
// 

NS_IMETHODIMP 
AjaxListener::StartObserving(void)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (obsService && NS_SUCCEEDED(rv)) {
	rv = obsService->AddObserver(this, "http-on-modify-request", PR_FALSE);
    }

    return rv;
}

NS_IMETHODIMP 
AjaxListener::StopObserving()
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (obsService && NS_SUCCEEDED(rv)) {
	rv = obsService->RemoveObserver(this, "http-on-modify-request");
    }

    return rv;
}


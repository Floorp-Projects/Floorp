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

#include <stdlib.h>

#include "AjaxListener.h"
#include "EmbedProgress.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsIXMLHTTPRequest.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"

#include "NativeBrowserControl.h"
#include "NativeWrapperFactory.h"
#include "HttpHeaderVisitorImpl.h"

#include "jni_util.h"

#define LOADSTR NS_LITERAL_STRING("load")
#define ERRORSTR NS_LITERAL_STRING("error")

NS_IMPL_ISUPPORTS2(AjaxListener, 
		   nsIObserver,
		   nsIDOMEventListener)

AjaxListener::AjaxListener(EmbedProgress *owner, 
			   JNIEnv *env) : 
    mOwner(owner),
    mJNIEnv(env),
    mIsObserving(PR_FALSE)
{
}

AjaxListener::~AjaxListener()
{
    mOwner->RemoveAjaxListener();
    mJNIEnv = nsnull;
    mOwner = nsnull;
    mIsObserving = PR_FALSE;
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
			rv = ObserveAjax(request, channel, ajax, 
					 AJAX_START);
			if (NS_SUCCEEDED(rv)) {
			    // Hook ourselves up as a DOMEventListener
			    nsCOMPtr<nsIDOMEventTarget> domTarget = 
				do_QueryInterface(ajax, &rv);
			    if (NS_SUCCEEDED(rv) && domTarget) {
				rv = domTarget->AddEventListener(LOADSTR, 
								 this,
								 PR_TRUE);
				if (NS_SUCCEEDED(rv)) {
				    rv = domTarget->AddEventListener(ERRORSTR, 
								     this,
								     PR_FALSE);
				}
			    }
			}
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
// Methods from nsIDOMEventListener
//

NS_IMETHODIMP 
AjaxListener::HandleEvent(nsIDOMEvent *event)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIXMLHttpRequest> ajax;
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    if (NS_SUCCEEDED(rv = event->GetTarget(getter_AddRefs(eventTarget)))) {
	ajax = do_QueryInterface(eventTarget);
	if (ajax) {
	    nsCOMPtr<nsIChannel> channel;
	    nsCOMPtr<nsIHttpChannel> httpChannel;
	    nsCOMPtr<nsIRequest> request = nsnull;
	    nsAutoString type;
	    AJAX_STATE state; 
	    
	    rv = ajax->GetChannel(getter_AddRefs(channel));
	    if (NS_SUCCEEDED(rv) && channel) {
		httpChannel = do_QueryInterface(channel);
		if (httpChannel) {
		    request = do_QueryInterface(channel);
		    if (request) {
			rv = event->GetType(type);
			if (NS_SUCCEEDED(rv)) {
			    state = type.EqualsIgnoreCase("load", 4) ? 
				AJAX_END : AJAX_ERROR;
			    rv = ObserveAjax(request, httpChannel, ajax, 
					     state);
			}
		    }
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
    if (mIsObserving) {
	return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (obsService && NS_SUCCEEDED(rv)) {
	rv = obsService->AddObserver(this, "http-on-modify-request", PR_FALSE);
	if (NS_SUCCEEDED(rv)) {
	    mIsObserving = PR_TRUE;
	}
    }

    return rv;
}

NS_IMETHODIMP 
AjaxListener::StopObserving()
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    if (!mIsObserving) {
	return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (obsService && NS_SUCCEEDED(rv)) {
	mIsObserving = PR_FALSE;
	rv = obsService->RemoveObserver(this, "http-on-modify-request");
    }

    return rv;
}

NS_IMETHODIMP 
AjaxListener::ObserveAjax(nsIRequest *request,
			  nsIHttpChannel *channel,
			  nsIXMLHttpRequest *ajax,
			  AJAX_STATE state)
{
    nsresult rv = NS_OK;
    NativeBrowserControl *browserControl = mOwner->GetOwner();
    NativeWrapperFactory *wrapperFactory = 
	browserControl->GetWrapperFactory();
    ShareInitContext *pShareContext = &(wrapperFactory->shareContext);
    jobject properties = ::util_NewGlobalRef(mJNIEnv,
	 ::util_CreatePropertiesObject(mJNIEnv, (jobject)pShareContext));    
    PRUint32 responseStatus = 0;
    PRInt32 readyState  = 0;
    char buf[20];
    jstring jStr = nsnull;
    nsCOMPtr<nsIDOMDocument> domDocument;
    jlong documentLong = nsnull;
    jclass clazz = nsnull;
    jmethodID mid = nsnull;
    jobject javaDOMDocument;

    browserControl->ContentStateChange();

    nsXPIDLCString uriString;
    nsCAutoString cstr;
    EmbedProgress::RequestToURIString(request, getter_Copies(uriString));
    const char * uriCStr = (const char *) uriString;
    // don't report "about:" URL events.
    if (uriString && 5 < uriString.Length() && 
	0 == strncmp("about:", uriCStr, 6)) {
	return NS_OK;
    }

    // store the request URI
    jStr = (jstring) ::util_NewGlobalRef(mJNIEnv,
	     ::util_NewStringUTF(mJNIEnv, 
				 (nsnull != uriCStr ? uriCStr : "")));
    ::util_StoreIntoPropertiesObject(mJNIEnv, properties, URI_VALUE, jStr,
				     (jobject) pShareContext);


    // store the headers
    HttpHeaderVisitorImpl *visitor = 
	new HttpHeaderVisitorImpl(mJNIEnv, properties, 
				  (jobject) pShareContext);
    if (AJAX_START == state) {
	channel->VisitRequestHeaders(visitor);
    }
    else {
	channel->VisitResponseHeaders(visitor);
    }
    delete visitor;

    // store the request method
    if (NS_SUCCEEDED(rv = channel->GetRequestMethod(cstr))) {
	jStr = 
	    (jstring) ::util_NewGlobalRef(mJNIEnv, 
			       ::util_NewStringUTF(mJNIEnv, cstr.get()));
	
	::util_StoreIntoPropertiesObject(mJNIEnv, properties, 
					 METHOD_VALUE, jStr,
					 (jobject) pShareContext);
    }

    // store the ready state
    if (NS_SUCCEEDED(rv = ajax->GetReadyState(&readyState))) {
        WC_ITOA(readyState, buf, 10);
        jStr = ::util_NewStringUTF(mJNIEnv, buf);
        ::util_StoreIntoPropertiesObject(mJNIEnv, properties, READY_STATE_KEY,
                                         (jobject) jStr, 
                                         (jobject) pShareContext);
    }

    if (AJAX_END == state || AJAX_ERROR == state) {
	nsAutoString autoString;
	// store the response status
	if (NS_SUCCEEDED(rv = ajax->GetStatus(&responseStatus))){
	    if (NS_SUCCEEDED(rv = ajax->GetStatusText(cstr))) {
		autoString.AppendInt(responseStatus);
		autoString.AppendWithConversion(" ");
		autoString.AppendWithConversion(cstr.get());
		
		jStr = (jstring) ::util_NewGlobalRef(mJNIEnv, 
		       ::util_NewString(mJNIEnv, 
					autoString.get(), 
					autoString.Length()));
		
		::util_StoreIntoPropertiesObject(mJNIEnv, properties, 
						 STATUS_VALUE, jStr,
						 (jobject) pShareContext);
		
	    }
	}

	// store the response text
	if (NS_SUCCEEDED(rv = ajax->GetResponseText(autoString))) {
		jStr = (jstring) ::util_NewGlobalRef(mJNIEnv, 
		       ::util_NewString(mJNIEnv, 
					autoString.get(), 
					autoString.Length()));
		
		::util_StoreIntoPropertiesObject(mJNIEnv, properties, 
						 RESPONSE_TEXT_KEY, jStr,
						 (jobject) pShareContext);

	}

	// store the response xml
	if (NS_SUCCEEDED(rv=ajax->GetResponseXML(getter_AddRefs(domDocument)))){
	    documentLong = (jlong) domDocument.get();
	    
	    if (nsnull == (clazz = 
			 ::util_FindClass(mJNIEnv,
					  "org/mozilla/dom/DOMAccessor"))) {
		::util_ThrowExceptionToJava(mJNIEnv, "Exception: Can't get DOMAccessor class");
		return nsnull;
	    }
	    if (nsnull==(mid = 
			 mJNIEnv->GetStaticMethodID(clazz, "getNodeByHandle",
						    "(J)Lorg/w3c/dom/Node;"))){
		::util_ThrowExceptionToJava(mJNIEnv, "Exception: Can't get DOM Node.");
		return nsnull;
	    }
	    
	    javaDOMDocument = (jobject) 
		util_CallStaticObjectMethodlongArg(mJNIEnv, 
						   clazz, mid, 
						   documentLong);
	    javaDOMDocument = ::util_NewGlobalRef(mJNIEnv, javaDOMDocument);
	    if (nsnull != javaDOMDocument) {
		::util_StoreIntoPropertiesObject(mJNIEnv, properties, 
						 RESPONSE_XML_KEY, 
						 javaDOMDocument,
						 (jobject) pShareContext);
	    }

	}

    }

    DOCUMENT_LOADER_EVENT_MASK_NAMES maskValue = END_AJAX_EVENT_MASK;
    switch (state) {
    case AJAX_START:
	maskValue = START_AJAX_EVENT_MASK;
	break;
    case AJAX_END:
	maskValue = END_AJAX_EVENT_MASK;
	break;
    case AJAX_ERROR:
	maskValue = ERROR_AJAX_EVENT_MASK;
	break;
    default:
	PR_ASSERT(PR_FALSE);
	break;
    }

    jobject eventRegistration = nsnull;
    if (NS_SUCCEEDED(rv = mOwner->GetEventRegistration(&eventRegistration)) &&
	eventRegistration) {
	::util_SendEventToJava(nsnull, 
			       eventRegistration, 
			       DOCUMENT_LOAD_LISTENER_CLASSNAME,
			       DocumentLoader_maskValues[maskValue], 
			       properties);
    }
    
    return rv;
}


/*******************

    


**********/

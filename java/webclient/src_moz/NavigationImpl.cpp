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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 *               Brian Satterfield <bsatterf@atl.lmco.com>
 *               Anthony Sizer <sizera@yahoo.com>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_NavigationImpl.h"

#include "NavigationActionEvents.h"
#include "nsIServiceManagerUtils.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsNetCID.h"

#include "ns_util.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadURL
(JNIEnv *env, jobject obj, jint webShellPtr, jstring urlString)
{
    jobject			jobj = obj;

#if DEBUG_RAPTOR_CANVAS
    const char	*	urlChars = (char *) ::util_GetStringUTFChars(env, 
                                                                 urlString);
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("Native URL = \"%s\"\n", urlChars));
    }
    ::util_ReleaseStringUTFChars(env, urlString, urlChars);
#endif
    
    PRUnichar	*	urlStringChars = (PRUnichar *) ::util_GetStringChars(env,
                                                                         urlString);
    PRInt32         urlLength = (PRInt32) ::util_GetStringLength(env, urlString);
    
    if (::util_ExceptionOccurred(env)) {
	::util_ThrowExceptionToJava(env, "raptorWebShellLoadURL Exception: unable to extract Java string");
	if (urlStringChars != nsnull)
	  ::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
	return;
      }
    
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellLoadURL");
      if (urlStringChars != nsnull)
	::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
      return;
    }
    
    if (initContext->initComplete) {
      wsLoadURLEvent	* actionEvent = new wsLoadURLEvent(initContext->webNavigation, urlStringChars, urlLength);
      PLEvent			* event       = (PLEvent*) *actionEvent;
      
      ::util_PostEvent(initContext, event);
    }
    
    ::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadFromStream
(JNIEnv *env, jobject obj, jint webShellPtr, jobject stream, jstring uri, 
 jstring contentType, jint contentLength, jobject loadProperties)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;
    PRUnichar *uriStringUniChars = nsnull;
    PRInt32 uriStringUniCharsLength = -1;
    const char *contentTypeChars = nsnull;
    jobject globalStream = nsnull;
    jobject globalLoadProperties = nsnull;
    nsString *uriNsString = nsnull;
    wsLoadFromStreamEvent *actionEvent = nsnull;
    
    if (initContext == nsnull || !initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeLoadFromStream");
        return;
    }
    uriStringUniChars = (PRUnichar *) ::util_GetStringChars(env, uri);
    uriStringUniCharsLength = (PRInt32) ::util_GetStringLength(env, uri);
    contentTypeChars = (char *) ::util_GetStringUTFChars(env, contentType);
    if (!uriStringUniChars || !contentTypeChars) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: unable to convert java string to native format");
        goto NLFS_CLEANUP;
    }

    if (!(uriNsString = 
          new nsString(uriStringUniChars, uriStringUniCharsLength))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: unable to convert native string to nsString");
        goto NLFS_CLEANUP;
    }

    // the deleteGlobalRef is done in the wsLoadFromStream destructor
    if (!(globalStream = ::util_NewGlobalRef(env, stream))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: unable to create gloabal ref to stream");
        goto NLFS_CLEANUP;
    }

    if (loadProperties) {
        // the deleteGlobalRef is done in the wsLoadFromStream destructor
        if (!(globalLoadProperties = 
              ::util_NewGlobalRef(env, loadProperties))) {
            ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: unable to create gloabal ref to properties");
            goto NLFS_CLEANUP;
        }
    }

    if (!(actionEvent = new wsLoadFromStreamEvent(initContext,
                                                  (void *) globalStream,
                                                  *uriNsString,
                                                  contentTypeChars,
                                                  (PRInt32) contentLength,
                                                  (void *) 
                                                  globalLoadProperties))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: can't create wsLoadFromStreamEvent");
        goto NLFS_CLEANUP;
    }
    ::util_PostSynchronousEvent(initContext, (PLEvent *) *actionEvent);

 NLFS_CLEANUP:
    ::util_ReleaseStringChars(env, uri, (const jchar *) uriStringUniChars);
    ::util_ReleaseStringUTFChars(env, contentType, contentTypeChars);
    delete uriNsString;

    // note, the deleteGlobalRef for loadProperties happens in the
    // wsLoadFromStreamEvent destructor.
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativePost
(JNIEnv *env, jobject obj, jint webShellPtr, jstring absoluteURL, jstring target, jint postDataLength,
 jstring postData, jint postHeadersLength, jstring postHeaders)
{
    NativeBrowserControl *initContext      = (NativeBrowserControl *) webShellPtr;
    const PRUnichar     *urlChars         = nsnull;
    PRInt32             urlLen;
    const PRUnichar     *targetChars      = nsnull;
    PRInt32             targetLen;
    const char          *postDataChars    = nsnull;
    const char          *postHeadersChars = nsnull;
    char                *headersAndData   = nsnull;
    wsPostEvent         *actionEvent      = nsnull;
    nsresult rv = NS_OK;
    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID,
                                                     &rv);
    nsCOMPtr<nsIURI> uri;
    NS_ConvertUCS2toUTF8 uriACString(urlChars);

    if (!ioService || NS_FAILED(rv)) {
        return;
    }

    if (initContext == nsnull || !initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativePost");
        return;
    }

    urlChars         = (PRUnichar *) ::util_GetStringChars(env, absoluteURL);
    urlLen           = (PRInt32) ::util_GetStringLength(env, absoluteURL);

    if (::util_ExceptionOccurred(env)) {
        ::util_ThrowExceptionToJava(env, "nativePost Exception: unable to extract Java string");
      goto NPFS_CLEANUP;
    }

    if (target){
      targetChars         = (PRUnichar *) ::util_GetStringChars(env, target);
      targetLen           = (PRInt32) ::util_GetStringLength(env, target);
    }

    if (::util_ExceptionOccurred(env)) {
        ::util_ThrowExceptionToJava(env, "nativePost Exception: unable to extract Java string");
      goto NPFS_CLEANUP;
    }


    if (postDataLength > 0){
      postDataChars    = (char *) ::util_GetStringUTFChars(env, postData);
    }
    if (::util_ExceptionOccurred(env)) {
        ::util_ThrowExceptionToJava(env, "nativePost Exception: unable to extract Java string");
      goto NPFS_CLEANUP;
    }

    if (postHeadersLength > 0){
      postHeadersChars = (char *) ::util_GetStringUTFChars(env, postHeaders);
    }
    if (::util_ExceptionOccurred(env)) {
        ::util_ThrowExceptionToJava(env, "nativePost Exception: unable to extract Java string");
      goto NPFS_CLEANUP;
    }

    // if we have postHeaders, work around mozilla bug and prepend the
    // headers to the data.
    if (postHeadersChars && postDataChars) {
        headersAndData = new char[postHeadersLength + postDataLength + 1];
        if (headersAndData) {
            memcpy(headersAndData, postHeadersChars, postHeadersLength);
            memcpy((headersAndData + postHeadersLength),
                          postDataChars, postDataLength);
            headersAndData[postHeadersLength + postDataLength] = '\0';
            // free the existing postHeadersChars and postDataChars
            ::util_ReleaseStringUTFChars(env, postHeaders, postHeadersChars);
            postHeadersChars = nsnull;
            postHeadersLength = 0;
            ::util_ReleaseStringUTFChars(env, postData, postDataChars);
            postDataChars = nsnull;
            postDataLength = postHeadersLength + postDataLength;
        }
    }

    rv = ioService->NewURI(uriACString, nsnull, nsnull, 
                           getter_AddRefs(uri));
    if (!uri || NS_FAILED(rv)) {
        goto NPFS_CLEANUP;
    }


    if (!(actionEvent = new wsPostEvent(initContext,
                                        uri,
                                        targetChars,
                                        targetLen, 
                                        (PRInt32) postDataLength,
                               headersAndData ? headersAndData : postDataChars,
                                        (PRInt32) postHeadersLength,
                                        postHeadersChars))) {

        ::util_ThrowExceptionToJava(env, "Exception: nativePost: can't create wsPostEvent");
        goto NPFS_CLEANUP;
    }

    ::util_PostSynchronousEvent(initContext, (PLEvent *) *actionEvent);

 NPFS_CLEANUP:
    if (urlChars != nsnull)
        ::util_ReleaseStringChars(env, absoluteURL, (const jchar *) urlChars);
    if (targetChars != nsnull)
        ::util_ReleaseStringChars(env, target, (const jchar *) targetChars);
    if (postDataChars != nsnull)
        ::util_ReleaseStringUTFChars(env, postData, postDataChars);
    if (postHeadersChars != nsnull)
        ::util_ReleaseStringUTFChars(env, postHeaders, postHeadersChars);
    if (headersAndData != nsnull)
        delete [] headersAndData;
    return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeRefresh
(JNIEnv *env, jobject obj, jint webShellPtr, jlong loadFlags)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRefresh");
		return;
	}

	if (initContext->initComplete) {
		wsRefreshEvent	* actionEvent = new wsRefreshEvent(initContext->webNavigation, (PRInt32) loadFlags);
        PLEvent	   	* event       = (PLEvent*) *actionEvent;

        ::util_PostEvent(initContext, event);

		return;
	}

	return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeStop
(JNIEnv *env, jobject obj, jint webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellStop");
		return;
	}

	if (initContext->initComplete) {
		wsStopEvent		* actionEvent = new wsStopEvent(initContext->webNavigation);
        PLEvent			* event       = (PLEvent*) *actionEvent;

        ::util_PostEvent(initContext, event);
	}
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeSetPrompt
(JNIEnv *env, jobject obj, jint webShellPtr, jobject userPrompt)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeSetPrompt");
		return;
	}

	if (userPrompt == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null properties passed to nativeSetPrompt");
		return;
	}

	if (!initContext->initComplete) {
        return;
    }
    
    // IMPORTANT: do the DeleteGlobalRef when we set a new prompt!
    
    PR_ASSERT(initContext->browserContainer);
    

    wsSetPromptEvent		* actionEvent = new wsSetPromptEvent(initContext->browserContainer, userPrompt);
    PLEvent			* event       = (PLEvent*) *actionEvent;
    ::util_PostSynchronousEvent(initContext, event);

}


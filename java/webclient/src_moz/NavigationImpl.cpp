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

#include "nsIServiceManagerUtils.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsIWebNavigation.h"
#include "nsNetCID.h"

#include "NativeBrowserControl.h"
#include "NavigationActionEvents.h"
#include "ns_util.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadURL
(JNIEnv *env, jobject obj, jint nativeBCPtr, jstring urlString)
{
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

    if (::util_ExceptionOccurred(env)) {
        ::util_ThrowExceptionToJava(env, "raptorWebShellLoadURL Exception: unable to extract Java string");
        if (urlStringChars != nsnull)
            ::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
        return;
    }
    
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null passed to nativeLoadURL");
      if (urlStringChars != nsnull)
          ::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
      return;
    }
    
    nsresult rv = 
        nativeBrowserControl->mNavigation->LoadURI(urlStringChars,
                                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                                   nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't load URL");
    }
    ::util_ReleaseStringChars(env, urlString, (const jchar *) urlStringChars);
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeLoadFromStream
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject stream, jstring uri, 
 jstring contentType, jint contentLength, jobject loadProperties)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    PRUnichar *uriStringUniChars = nsnull;
    PRInt32 uriStringUniCharsLength = -1;
    const char *contentTypeChars = nsnull;
    jobject globalStream = nsnull;
    jobject globalLoadProperties = nsnull;
    nsString *uriNsString = nsnull;
    wsLoadFromStreamEvent *actionEvent = nsnull;
    
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeLoadFromStream");
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
    
    // the deleteGlobalRef is done in the InputStreamShim destructor
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
    
    if (!(actionEvent = new wsLoadFromStreamEvent(nativeBrowserControl,
                                                  (void *) globalStream,
                                                  *uriNsString,
                                                  contentTypeChars,
                                                  (PRInt32) contentLength,
                                                  (void *) 
                                                  globalLoadProperties))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeLoadFromStream: can't create wsLoadFromStreamEvent");
        goto NLFS_CLEANUP;
    }
    ::util_PostSynchronousEvent((PLEvent *) *actionEvent);

 NLFS_CLEANUP:
    ::util_ReleaseStringChars(env, uri, (const jchar *) uriStringUniChars);
    ::util_ReleaseStringUTFChars(env, contentType, contentTypeChars);
    delete uriNsString;

    // note, the deleteGlobalRef for loadProperties happens in the
    // wsLoadFromStreamEvent destructor.
}

    /**********************



JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativePost
(JNIEnv *env, jobject obj, jint nativeBCPtr, jstring absoluteURL, jstring target, jint postDataLength,
 jstring postData, jint postHeadersLength, jstring postHeaders)
{
    NativeBrowserControl *nativeBrowserControl      = (NativeBrowserControl *) nativeBCPtr;
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

    if (nativeBrowserControl == nsnull || !nativeBrowserControl->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativePost");
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


    if (!(actionEvent = new wsPostEvent(nativeBrowserControl,
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

    ::util_PostSynchronousEvent(nativeBrowserControl, (PLEvent *) *actionEvent);

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
*********************/

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeRefresh
(JNIEnv *env, jobject obj, jint nativeBCPtr, jlong loadFlags)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellRefresh");
		return;
	}

    nsresult rv = 
        nativeBrowserControl->mNavigation->Reload(loadFlags);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't refresh");
    }

	return;
}


JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeStop
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null passed to nativeLoadURL");
        return;
    }
    
    nsresult rv = 
        nativeBrowserControl->mNavigation->Stop(nsIWebNavigation::STOP_ALL);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't stop load");
    }
}

/***********************

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NavigationImpl_nativeSetPrompt
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject userPrompt)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeSetPrompt");
		return;
	}

	if (userPrompt == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null properties passed to nativeSetPrompt");
		return;
	}

	if (!nativeBrowserControl->initComplete) {
        return;
    }
    
    // IMPORTANT: do the DeleteGlobalRef when we set a new prompt!
    
    PR_ASSERT(nativeBrowserControl->browserContainer);
    

    wsSetPromptEvent		* actionEvent = new wsSetPromptEvent(nativeBrowserControl->browserContainer, userPrompt);
    PLEvent			* event       = (PLEvent*) *actionEvent;
    ::util_PostSynchronousEvent(nativeBrowserControl, event);

}

********************/

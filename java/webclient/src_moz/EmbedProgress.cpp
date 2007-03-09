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
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "EmbedProgress.h"

#include <nsXPIDLString.h>
#include <nsIChannel.h>
#include <nsIHttpChannel.h>
#include <nsIUploadChannel.h>
#include <nsIInputStream.h>
#include <nsISeekableStream.h>
#include <nsIHttpHeaderVisitor.h>

#include "nsIURI.h"
#include "nsCRT.h"

#include "nsString.h"

#include "NativeBrowserControl.h"

#include "HttpHeaderVisitorImpl.h"
#include "AjaxListener.h"

#include "ns_globals.h" // for prLogModuleInfo

EmbedProgress::EmbedProgress(void) : 
    mCapturePageInfo(JNI_FALSE),
    mAjaxListener(nsnull)
{
  mOwner = nsnull;
  mEventRegistration = nsnull;
}

EmbedProgress::~EmbedProgress()
{ 
    if (nsnull != mEventRegistration) {
	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
	::util_DeleteGlobalRef(env, mEventRegistration);
	mEventRegistration = nsnull;
    }
    RemoveAjaxListener();

}

NS_IMPL_ISUPPORTS2(EmbedProgress,
		   nsIWebProgressListener,
		   nsISupportsWeakReference)

nsresult
EmbedProgress::Init(NativeBrowserControl *aOwner)
{
  mOwner = aOwner;

  if (-1 == DocumentLoader_maskValues[0]) {
      util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
					      DocumentLoader_maskNames, 
					      DocumentLoader_maskValues,
					      nsnull);
  }

  return NS_OK;
}

nsresult
EmbedProgress::SetEventRegistration(jobject yourEventRegistration)
{
    nsresult rv = NS_OK;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    mEventRegistration = ::util_NewGlobalRef(env, yourEventRegistration);
    if (nsnull == mEventRegistration) {
        ::util_ThrowExceptionToJava(env, "Exception: EmbedProgress->SetEventRegistration(): can't create NewGlobalRef\n\tfor eventRegistration");
	rv = NS_ERROR_FAILURE;
    }
    // We get the listener here to ensure the mEventRegistration is 
    // properly passed to the AjaxListener ctor.
    AjaxListener *observer = nsnull;
    rv = GetAjaxListener(&observer);
    if (observer && NS_SUCCEEDED(rv)) {
	if (mCapturePageInfo) {
	    observer->StartObserving();
	}
	else {
	    observer->StopObserving();
	}
    }

    return rv;
}

nsresult 
EmbedProgress::SetCapturePageInfo(jboolean newState)
{
    mCapturePageInfo = newState;
    AjaxListener *observer = nsnull;
    nsresult rv = GetAjaxListener(&observer);
    if (observer && NS_SUCCEEDED(rv)) {
	if (mCapturePageInfo) {
	    observer->StartObserving();
	}
	else {
	    observer->StopObserving();
	}
    }

    return rv;
}

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
			     nsIRequest     *aRequest,
			     PRUint32        aStateFlags,
			     nsresult        aStatus)
{
    // hook up listeners for this request.
    mOwner->ContentStateChange();
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsXPIDLCString uriString;
    nsCAutoString cstr;
    nsresult rv;
    RequestToURIString(aRequest, getter_Copies(uriString));
    const char * uriCStr = (const char *) uriString;
    // don't report "about:" URL events.
    if (uriString && 5 < uriString.Length() && 
	0 == strncmp("about:", uriCStr, 6)) {
	return NS_OK;
    }

    jstring uriJstr = (jstring) ::util_NewGlobalRef(env,
					  ::util_NewStringUTF(env, (nsnull != uriCStr 
								    ? uriCStr : "")));

    jobject properties = ::util_NewGlobalRef(env,
					     ::util_CreatePropertiesObject(env, 
									   (jobject)
									   &(mOwner->GetWrapperFactory()->shareContext)));
    ::util_StoreIntoPropertiesObject(env, properties, URI_VALUE, uriJstr,
				     (jobject)
				     &(mOwner->GetWrapperFactory()->shareContext));
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedProgress::OnStateChange: URI: %s\n",
	    (const char *) uriString));
    
    // 
    // document states
    //
    nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest);

    // if we've got the start flag, emit the signal
    if ((aStateFlags & STATE_IS_NETWORK) && 
	(aStateFlags & STATE_START)) {

	mOwner->TopLevelFocusIn();
	
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	       ("EmbedProgress::OnStateChange: START_DOCUMENT_LOAD\n"));

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK], 
			     properties);
    }

    // and for stop, too
    if ((aStateFlags & STATE_IS_NETWORK) && 
	(aStateFlags & STATE_STOP)) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	       ("EmbedProgress::OnStateChange: END_DOCUMENT_LOAD\n"));

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK], 
			     properties);
	
    }

    //
    // request states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_REQUEST)) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	       ("EmbedProgress::OnStateChange: START_URL_LOAD\n"));
	if (channel && mCapturePageInfo) {
	    HttpHeaderVisitorImpl *visitor = 
		new HttpHeaderVisitorImpl(env, 
					  properties, (jobject)
					  &(mOwner->GetWrapperFactory()->shareContext));
	    channel->VisitRequestHeaders(visitor);
	    delete visitor;
	    // store the request method
	    if (NS_SUCCEEDED(rv = channel->GetRequestMethod(cstr))) {
		jstring methodJStr = (jstring) ::util_NewGlobalRef(env, 
								   ::util_NewStringUTF(env, cstr.get()));
		
		::util_StoreIntoPropertiesObject(env, properties, 
						 METHOD_VALUE, methodJStr,
						 (jobject)
						 &(mOwner->GetWrapperFactory()->shareContext));
	    }
	}

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK], 
			     properties);
    }

    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_REQUEST)) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	       ("EmbedProgress::OnStateChange: END_URL_LOAD\n"));

	if (channel && mCapturePageInfo) {
	    // store the response headers
	    HttpHeaderVisitorImpl *visitor = 
		new HttpHeaderVisitorImpl(env, 
					  properties, (jobject)
					  &(mOwner->GetWrapperFactory()->shareContext));
	    channel->VisitResponseHeaders(visitor);
	    delete visitor;
	    // store the request method
	    if (NS_SUCCEEDED(rv = channel->GetRequestMethod(cstr))) {
		jstring methodJStr = (jstring) ::util_NewGlobalRef(env, 
								   ::util_NewStringUTF(env, cstr.get()));
		
		::util_StoreIntoPropertiesObject(env, properties, 
						 METHOD_VALUE, methodJStr,
						 (jobject)
						 &(mOwner->GetWrapperFactory()->shareContext));
	    }

	    // store the response status
	    PRUint32 responseStatus;
	    if (NS_SUCCEEDED(rv =channel->GetResponseStatus(&responseStatus))){
		if (NS_SUCCEEDED(rv=channel->GetResponseStatusText(cstr))) {
		    nsAutoString autoStatus;
		    autoStatus.AppendInt(responseStatus);
		    autoStatus.AppendWithConversion(" ");
		    autoStatus.AppendWithConversion(cstr.get());

		    jstring statusJStr = (jstring) ::util_NewGlobalRef(env, 
								       ::util_NewString(env, autoStatus.get(), autoStatus.Length()));
		    
		    ::util_StoreIntoPropertiesObject(env, properties, 
						     STATUS_VALUE, statusJStr,
						     (jobject)
						     &(mOwner->GetWrapperFactory()->shareContext));
		    
		}
	    }

	    // If there is an upload stream, store it as well
	    nsCOMPtr<nsIUploadChannel> upload = do_QueryInterface(channel);
	    if (upload) {
		nsIInputStream *uploadStream = nsnull;
		if (NS_SUCCEEDED(rv = upload->GetUploadStream(&uploadStream)) 
		    && uploadStream) {
		    jint pStream;
		    jclass clazz;
		    jobject streamObj;
		    jmethodID jID;
		    pStream = (jint) uploadStream;
		    uploadStream->AddRef();
		    // rewind upload stream
		    nsCOMPtr<nsISeekableStream> seekable = 
			do_QueryInterface(uploadStream);
		    if (seekable) {
			seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
		    }
		    
		    if (clazz = env->FindClass("org/mozilla/webclient/impl/wrapper_native/NativeInputStream")) {
			if (jID = env->GetMethodID(clazz, "<init>", "(I)V")) {
			    if (streamObj = env->NewObject(clazz,jID,pStream)){
			       if (streamObj = ::util_NewGlobalRef(env,
								   streamObj)){
				   ::util_StoreIntoPropertiesObject(env, 
								    properties,
								    REQUEST_BODY_VALUE, 
								    streamObj,
								    (jobject)
								    &(mOwner->GetWrapperFactory()->shareContext));
			       }
			    }
			}
		    }
		}
	    }
	}

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK], 
			     properties);
    }

    //    ::util_DestroyPropertiesObject(env, properties, nsnull);

    //    ::util_DeleteStringUTF(env, uriJstr);
    
    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnProgressChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRInt32         aCurSelfProgress,
				PRInt32         aMaxSelfProgress,
				PRInt32         aCurTotalProgress,
				PRInt32         aMaxTotalProgress)
{
    
    nsXPIDLCString uriString;
    RequestToURIString(aRequest, getter_Copies(uriString));
    const char * uriCStr = (const char *) uriString;
    PRInt32 percentComplete = 0;
    nsCAutoString name;
    nsAutoString autoName;
    nsresult rv = NS_OK;

    // don't report "about:" URL events.
    if (uriString && 5 < uriString.Length() && 
	0 == strncmp("about:", (const char *) uriString, 6)) {
	return NS_OK;
    }

    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    jstring uriJstr = (jstring) ::util_NewGlobalRef(env,
						    ::util_NewStringUTF(env, (nsnull != uriCStr 
									      ? uriCStr : "")));
    
    jobject properties = ::util_NewGlobalRef(env,
					     ::util_CreatePropertiesObject(env, 
									   (jobject)
									   &(mOwner->GetWrapperFactory()->shareContext)));
    ::util_StoreIntoPropertiesObject(env, properties, URI_VALUE, uriJstr,
				     (jobject)
				     &(mOwner->GetWrapperFactory()->shareContext));
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedProgress::OnProgressChange: URI: %s\n\taCurSelfProgress: %d\n\taMaxSelfProgress: %d\n\taCurTotalProgress: %d\n\taMaxTotalProgress: %d\n",
	    (const char *) uriString, aCurSelfProgress, aMaxSelfProgress,
	    aCurTotalProgress, aMaxTotalProgress));

    // PENDING(edburns): Allow per fetch progress reporting.  Right now
    // we only have coarse grained support.
    if (0 < aMaxTotalProgress) {
        percentComplete = aCurTotalProgress / aMaxTotalProgress;
    }
    
    if (NS_FAILED(rv = aRequest->GetName(name))) {
        return rv;
    }
    autoName.AssignWithConversion(name.get());
    // build up the string to be sent
    autoName.AppendWithConversion(" ");
    autoName.AppendInt(percentComplete);
    autoName.AppendWithConversion("%");
    
    jstring msgJStr = (jstring) ::util_NewGlobalRef(env, 
						    ::util_NewString(env, autoName.get(), autoName.Length()));
    
    ::util_StoreIntoPropertiesObject(env, properties, MESSAGE_VALUE, msgJStr,
				     (jobject)
				     &(mOwner->GetWrapperFactory()->shareContext));

    util_SendEventToJava(nsnull, 
			 mEventRegistration, 
			 DOCUMENT_LOAD_LISTENER_CLASSNAME,
			 DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK], 
			 properties);
    
    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnLocationChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				nsIURI         *aLocation)
{
  nsCAutoString newURI;
  NS_ENSURE_ARG_POINTER(aLocation);
  aLocation->GetSpec(newURI);

  PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	 ("EmbedProgress::OnLocationChange: URI: %s\n",
	  (const char *) newURI.get()));


  /**********
  mOwner->SetURI(newURI.get());
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[LOCATION]);
  *******************/
  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStatusChange(nsIWebProgress  *aWebProgress,
			      nsIRequest      *aRequest,
			      nsresult         aStatus,
			      const PRUnichar *aMessage)
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    nsXPIDLCString uriString;
    RequestToURIString(aRequest, getter_Copies(uriString));
    const char * uriCStr = (const char *) uriString;

    // don't report "about:" URL events.
    if (uriString && 5 < uriString.Length() && 
	0 == strncmp("about:", uriString, 6)) {
	return NS_OK;
    }

    jstring uriJstr = (jstring) ::util_NewGlobalRef(env,
						    ::util_NewStringUTF(env, (nsnull != uriCStr 
									      ? uriCStr : "")));
    
    jobject properties = ::util_NewGlobalRef(env,
					     ::util_CreatePropertiesObject(env, 
									   (jobject)
									   &(mOwner->GetWrapperFactory()->shareContext)));
    ::util_StoreIntoPropertiesObject(env, properties, URI_VALUE, uriJstr,
				     (jobject)
				     &(mOwner->GetWrapperFactory()->shareContext));
    
    jstring msgJstr = (jstring) ::util_NewGlobalRef(env,
						    ::util_NewString(env, aMessage,
								     nsCRT::strlen(aMessage)));
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedProgress::OnStatusChange: URI: %s\n",
	    (const char *) uriString));

    util_SendEventToJava(nsnull, 
			 mEventRegistration, 
			 DOCUMENT_LOAD_LISTENER_CLASSNAME,
			 DocumentLoader_maskValues[STATUS_URL_LOAD_EVENT_MASK], 
			 properties);
    
    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRUint32         aState)
{
    nsXPIDLCString uriString;
    RequestToURIString(aRequest, getter_Copies(uriString));

    // don't report "about:" URL events.
    if (uriString && 5 < uriString.Length() && 
	0 == strncmp("about:", (const char *) uriString, 6)) {
	return NS_OK;
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedProgress::OnSecurityChange: URI: %s\n",
	    (const char *) uriString));

    /**************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[SECURITY_CHANGE],
		  NS_STATIC_CAST(void *, aRequest),
		  aState);
    **********/
  return NS_OK;
}

NativeBrowserControl* EmbedProgress::GetOwner()
{
    return mOwner;
}

NS_IMETHODIMP
EmbedProgress::GetAjaxListener(AjaxListener* *result)
{
    if (nsnull == result) {
	return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_ERROR_FAILURE;

    if (nsnull == mAjaxListener) {
	JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
	mAjaxListener = new AjaxListener(this, env);
    }
    *result = mAjaxListener;
    rv = NS_OK;
    
    return rv;
}

NS_IMETHODIMP
EmbedProgress::RemoveAjaxListener(void) 
{
    nsresult rv = NS_OK;
    AjaxListener *observer = nsnull;
    rv = GetAjaxListener(&observer);
    if (NS_SUCCEEDED(rv) && observer) {
	observer->StopObserving();
	mAjaxListener = nsnull;
    }

    return rv;
}

NS_IMETHODIMP
EmbedProgress::GetEventRegistration(jobject *result)
{
    if (nsnull == result) {
	return NS_ERROR_NULL_POINTER;
    }
    *result = mEventRegistration;
    return NS_OK;
}


/* static */
void
EmbedProgress::RequestToURIString(nsIRequest *aRequest, char **aString)
{
  // is it a channel
  nsCOMPtr<nsIChannel> channel;
  channel = do_QueryInterface(aRequest);
  if (!channel)
    return;
  
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  if (!uri)
    return;
  
  nsCAutoString uriString;
  uri->GetSpec(uriString);

  *aString = strdup(uriString.get());
}

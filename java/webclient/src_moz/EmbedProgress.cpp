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
#include <nsIHttpHeaderVisitor.h>

#include "nsIURI.h"
#include "nsCRT.h"

#include "nsString.h"

#include "NativeBrowserControl.h"

#include "ns_globals.h" // for prLogModuleInfo

EmbedProgress::EmbedProgress(void)
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
					      DocumentLoader_maskValues);
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

    return rv;
}

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
			     nsIRequest     *aRequest,
			     PRUint32        aStateFlags,
			     nsresult        aStatus)
{
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    nsXPIDLCString uriString;
    RequestToURIString(aRequest, getter_Copies(uriString));
    const char * uriCStr = (const char *) uriString;
    jstring uriJstr = ::util_NewStringUTF(env, (nsnull != uriCStr 
						? uriCStr : ""));
    
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
	
	if (channel) {
	    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
		   ("EmbedProgress::OnStateChange: have nsIHttpChannel at START_DOCUMENT_LOAD\n"));
	}

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK], 
			     uriJstr);
    }

    // and for stop, too
    if ((aStateFlags & STATE_IS_NETWORK) && 
	(aStateFlags & STATE_STOP)) {
	if (channel) {
	    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
		   ("EmbedProgress::OnStateChange: have nsIHttpChannel at END_DOCUMENT_LOAD\n"));
	}

	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK], 
			     uriJstr);
	
    }

    //
    // request states
    //
    if ((aStateFlags & STATE_START) && (aStateFlags & STATE_IS_REQUEST)) {
	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK], 
			     uriJstr);
    }

    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_REQUEST)) {
	util_SendEventToJava(nsnull, 
			     mEventRegistration, 
			     DOCUMENT_LOAD_LISTENER_CLASSNAME,
			     DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK], 
			     uriJstr);
    }

    ::util_DeleteStringUTF(env, uriJstr);
    
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
    PRInt32 percentComplete = 0;
    nsCAutoString name;
    nsAutoString autoName;
    nsresult rv = NS_OK;
    
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
    
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    jstring msgJStr = ::util_NewString(env, autoName.get(), autoName.Length());
    util_SendEventToJava(nsnull, 
			 mEventRegistration, 
			 DOCUMENT_LOAD_LISTENER_CLASSNAME,
			 DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK], 
			 msgJStr);
    
    if (msgJStr) {
        ::util_DeleteString(env, msgJStr);
    }
  
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
    jstring msgJstr = ::util_NewString(env, aMessage,
				       nsCRT::strlen(aMessage));
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	   ("EmbedProgress::OnStatusChange: URI: %s\n",
	    (const char *) uriString));

    util_SendEventToJava(nsnull, 
			 mEventRegistration, 
			 DOCUMENT_LOAD_LISTENER_CLASSNAME,
			 DocumentLoader_maskValues[STATUS_URL_LOAD_EVENT_MASK], 
			 msgJstr);
    
    if (msgJstr) {
	::util_DeleteString(env, msgJstr);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRUint32         aState)
{
    nsXPIDLCString uriString;
    RequestToURIString(aRequest, getter_Copies(uriString));
    
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

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

#include "nsIURI.h"
#include "nsCRT.h"

#include "nsString.h"

#include "NativeBrowserControl.h"

#include "ns_globals.h" // for prLogModuleInfo

EmbedProgress::EmbedProgress(void)
{
  mOwner = nsnull;
}

EmbedProgress::~EmbedProgress()
{
}

NS_IMPL_ISUPPORTS2(EmbedProgress,
		   nsIWebProgressListener,
		   nsISupportsWeakReference)

nsresult
EmbedProgress::Init(NativeBrowserControl *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress,
			     nsIRequest     *aRequest,
			     PRUint32        aStateFlags,
			     nsresult        aStatus)
{
  // get the uri for this request
  nsXPIDLCString uriString;
  RequestToURIString(aRequest, getter_Copies(uriString));
  nsString tmpString;
  tmpString.AssignWithConversion(uriString);

  PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	 ("EmbedProgress::OnStateChange: URI: %s\n",
	  (const char *) uriString));

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: interpreting flags\n"));

    if (aStateFlags & STATE_IS_REQUEST) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_IS_REQUEST\n"));
    }
    if (aStateFlags & STATE_IS_DOCUMENT) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_IS_DOCUMENT\n"));
    }
    if (aStateFlags & STATE_IS_NETWORK) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_IS_NETWORK\n"));
    }
    if (aStateFlags & STATE_IS_WINDOW) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_IS_WINDOW\n"));
    }

    if (aStateFlags & STATE_START) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_START\n"));
    }
    if (aStateFlags & STATE_REDIRECTING) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_REDIRECTING\n"));
    }
    if (aStateFlags & STATE_TRANSFERRING) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_TRANSFERRING\n"));
    }
    if (aStateFlags & STATE_NEGOTIATING) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_NEGOTIATING\n"));
    }
    if (aStateFlags & STATE_STOP) {
	PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: STATE_STOP\n"));
    }

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, ("debug: edburns: EmbedProgress::OnStateChange: done interpreting flags\n"));

  // give the widget a chance to attach any listeners
    //  mOwner->ContentStateChange();
  // if we've got the start flag, emit the signal
  if ((aStateFlags & STATE_IS_NETWORK) && 
      (aStateFlags & STATE_START))
  {
      //    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
      //		    moz_embed_signals[NET_START]);
  }

  /************
  // is it the same as the current URI?
  if (mOwner->mURI.Equals(tmpString))
  {
    // for people who know what they are doing
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		    moz_embed_signals[NET_STATE],
		    aStateFlags, aStatus);
  }
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[NET_STATE_ALL],
		  (gpointer)(const char *)uriString,
		  (gint)aStateFlags, (gint)aStatus);
  *************/
  // and for stop, too
  if ((aStateFlags & STATE_IS_NETWORK) && 
      (aStateFlags & STATE_STOP))
  {
      /************
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		    moz_embed_signals[NET_STOP]);
    // let our owner know that the load finished
    mOwner->ContentFinishedLoading();
      *********/
  }

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
  
  nsString tmpString;
  tmpString.AssignWithConversion(uriString);

  PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	 ("EmbedProgress::OnProgressChange: URI: %s\n\taCurSelfProgress: %d\n\taMaxSelfProgress: %d\n\taCurTotalProgress: %d\n\taMaxTotalProgress: %d\n",
	  (const char *) uriString, aCurSelfProgress, aMaxSelfProgress,
	  aCurTotalProgress, aMaxTotalProgress));
  
  // is it the same as the current uri?
  /***********
  if (mOwner->mURI.Equals(tmpString)) {
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		    moz_embed_signals[PROGRESS],
		    aCurTotalProgress, aMaxTotalProgress);
  }

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[PROGRESS_ALL],
		  (const char *)uriString,
		  aCurTotalProgress, aMaxTotalProgress);
  *******************/
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
  // need to make a copy so we can safely cast to a void *
  PRUnichar *tmpString = nsCRT::strdup(aMessage);

  nsXPIDLCString uriString;
  RequestToURIString(aRequest, getter_Copies(uriString));

  PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
	 ("EmbedProgress::OnStatusChange: URI: %s\n",
	  (const char *) uriString));

  /**************
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
		  moz_embed_signals[STATUS_CHANGE],
		  NS_STATIC_CAST(void *, aRequest),
		  NS_STATIC_CAST(int, aStatus),
		  NS_STATIC_CAST(void *, tmpString));
  ***********/

  nsMemory::Free(tmpString);

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

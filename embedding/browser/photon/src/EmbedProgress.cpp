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
 *   Brian Edmond <briane@qnx.com>
 */

#include "EmbedProgress.h"

#include <nsXPIDLString.h>
#include <nsIChannel.h>

#include <nsCWebBrowser.h>
#include <nsIComponentManager.h>
#include <nsIDocShellTreeItem.h>
#include "nsIWidget.h"
#include <nsString.h>
#include <nsError.h>
#include <nsIDNSService.h>
#include "nsReadableUtils.h"
#include <nsILocalFile.h>
#include <nsIRequestObserver.h>
#include <nsISocketTransportService.h>
#include <netCore.h>
#include "nsGfxCIID.h"

#include "nsIDOMWindow.h"

#include "EmbedWindow.h"
#include "EmbedPrivate.h"

#include "nsIURI.h"
#include "PtMozilla.h"

EmbedProgress::EmbedProgress(void)
{
	NS_INIT_ISUPPORTS();
  mOwner = nsnull;
  mSkipOnState = mDownloadDocument = 0;
}

EmbedProgress::~EmbedProgress()
{
}

NS_IMPL_ISUPPORTS2(EmbedProgress,
		   nsIWebProgressListener,
		   nsISupportsWeakReference)

nsresult
EmbedProgress::Init(EmbedPrivate *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP 
EmbedProgress::OnStateChange(nsIWebProgress *aWebProgress, 
				 nsIRequest 	*aRequest, 
				 PRUint32 		aStateFlags, 
				 nsresult 		aStatus)
{
	PtMozillaWidget_t 		*moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t 		*cb = NULL;
	PtCallbackInfo_t 		cbinfo;
	PtMozillaNetStateCb_t   state;

	// get the uri for this request
	nsXPIDLCString uriString;
	RequestToURIString(aRequest, getter_Copies(uriString));
	nsString tmpString;
	tmpString.AssignWithConversion(uriString);

	if( ( aStateFlags & STATE_IS_REQUEST ) && NS_FAILED( aStatus ) ) 
	{
		PtWebErrorCallback_t cbw;

		/* invoke the Pt_CB_WEB_ERROR in the client */
		cb = moz->web_error_cb;
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.reason = Pt_CB_MOZ_ERROR;
		cbinfo.cbdata = &cbw;

		memset( &cbw, 0, sizeof( PtWebErrorCallback_t ) );
		cbw.url = (char*)((const char *)uriString);
		cbw.description = "";

		cbw.type = Pt_WEB_ERROR_TOPVIEW;
		switch( aStatus ) 
		{
			case NS_ERROR_UNKNOWN_HOST:				
				cbw.reason = -12; 
				break;
			case NS_ERROR_NET_TIMEOUT: 				
				cbw.reason = -408; 
				break;
			case NS_ERROR_NO_CONTENT:					
				cbw.reason = -8; 
				break;
			case NS_ERROR_FILE_NOT_FOUND:			
				cbw.reason = -404; 
				break;
			case NS_ERROR_CONNECTION_REFUSED:	
				cbw.reason = -13; 
				break;
			default:		
				cbw.reason = -1; 
				break;
		}
		if( cbw.reason != -1) 
			PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);
		/* let it check for STATE_STOP */
	}

	memset(&cbinfo, 0, sizeof(cbinfo));

	if( aStateFlags & STATE_IS_NETWORK /* STATE_IS_REQUEST STATE_IS_DOCUMENT */ ) 
	{
    	if( aStateFlags & STATE_START ) 
		{
			cbinfo.reason = Pt_CB_MOZ_START;
			if( ( cb = moz->start_cb ) )
				PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
    	}
    	else if( aStateFlags & STATE_STOP ) 
		{
			PtWebCompleteCallback_t cbcomplete;

			cbinfo.reason = Pt_CB_MOZ_COMPLETE;
			cbinfo.cbdata = &cbcomplete;
			memset( &cbcomplete, 0, sizeof( PtWebCompleteCallback_t ) );
			cbcomplete.url = strdup( uriString );

			if( ( cb = moz->complete_cb ) )
				PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

			free( cbcomplete.url );

			if( moz->text_zoom != moz->actual_text_zoom ) {
				nsCOMPtr<nsIDOMWindow> domWindow;
				moz->EmbedRef->mWindow->mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
				if(domWindow) {
					domWindow->SetTextZoom( moz->text_zoom/100. );
					float vv;
					domWindow->GetTextZoom( &vv );
					moz->actual_text_zoom = (int) ( vv * 100 );
					}
				}
    	}
	}

	// invoke the raw status callbacks for page load status
	cbinfo.reason = Pt_CB_MOZ_NET_STATE;
	cbinfo.cbdata = &state;
	state.flags = aStateFlags;
	state.status = aStatus;
	//state.url = (const char *)uriString;
	state.url = NULL;
	char *statusMessage = "";
	PRInt32 flags = aStateFlags;

	if (flags & STATE_IS_REQUEST) 
	{
	  if (flags & STATE_REDIRECTING)
		statusMessage = "Redirecting to site:";
	  else if (flags & STATE_TRANSFERRING)
		statusMessage = "Receiving Data:";
	  else if (flags & STATE_NEGOTIATING)
	  statusMessage = "Waiting for authorization:";
	}

	if (flags & STATE_IS_DOCUMENT) 
	{
	  if (flags & STATE_START)
		statusMessage = "Loading site:";
	  else if (flags & STATE_STOP)
		statusMessage = "Finishing:";
	}
	state.message = statusMessage;
	if( ( cb = moz->net_state_cb ) && statusMessage[0])
		PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
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
	PtMozillaWidget_t       *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t        *cb;
	PtCallbackInfo_t        cbinfo;
	PtMozillaProgressCb_t   prog;
	PRInt32					cur, max;
	nsXPIDLCString 			uriString;
	if (!moz->progress_cb)
		return NS_OK;

	cur = aCurTotalProgress;
	max = aMaxTotalProgress;

	if (cur > max && max != -1 && max != 0)
		cur = max; // Progress complete

	cb = moz->progress_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_PROGRESS;
	cbinfo.cbdata = &prog;

	prog.cur = cur;
	prog.max = max;
	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

  	return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnLocationChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				nsIURI         *aLocation)
{
	NS_ENSURE_ARG_POINTER(aLocation);

	PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t    *cb = NULL;
	PtCallbackInfo_t    cbinfo;
	PtMozillaUrlCb_t    url;
	nsCAutoString newURI;

	aLocation->GetSpec(newURI);
	mOwner->SetURI(newURI.get());

	if (!moz->url_cb)
	    return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &url;
	cbinfo.reason = Pt_CB_MOZ_URL;
	url.url = (char *) newURI.get();
//	aLocation->GetSpec(&(url.url));
	cb = moz->url_cb;
	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
  	
	return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnStatusChange(nsIWebProgress  *aWebProgress,
			      nsIRequest      *aRequest,
			      nsresult         aStatus,
			      const PRUnichar *aMessage)
{
	PtMozillaWidget_t   	*moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
	PtCallbackList_t        *cb;
	PtCallbackInfo_t        cbinfo;
	PtMozillaInfoCb_t       info;
	nsAutoString Text ( aMessage );

	if (!moz->info_cb)
		return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &info;
	cbinfo.reason = Pt_CB_MOZ_INFO;
	cb = moz->info_cb;

	const char* msg = ToNewCString(Text);
	info.type = Pt_MOZ_INFO_CONNECT;
	info.status = 0;
	info.data = (char *)msg;
	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

	nsMemory::Free( (void*)msg );
  	return NS_OK;
}

NS_IMETHODIMP
EmbedProgress::OnSecurityChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRUint32         aState)
{
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

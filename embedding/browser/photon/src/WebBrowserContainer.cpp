/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "WebBrowserContainer.h"
#include "PhMozEmbedStream.h"
#include "PtMozilla.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIWindowCreator.h"
#include "nsIWindowWatcher.h"
#include "nsReadableUtils.h"

/* for OnStateChange error codes */
#include "nsError.h"
#include "nsIDNSService.h"
#include "nsISocketTransportService.h"

CWebBrowserContainer::CWebBrowserContainer(PtWidget_t *pOwner)
{
	NS_INIT_REFCNT();
	m_pOwner = pOwner;
	m_pCurrentURI = nsnull;
	mDoingStream = PR_FALSE;
	mOffset = 0;
	mSkipOnState = mDownloadDocument = 0;
}


CWebBrowserContainer::~CWebBrowserContainer()
{
}


void CWebBrowserContainer::InvokeInfoCallback(int type, unsigned int status, char *data)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *)m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaInfoCb_t 	info;

	if (!moz->info_cb)
		return;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &info;
	cbinfo.reason = Pt_CB_MOZ_INFO;
	cb = moz->info_cb;

	info.type = type;
	info.status = status;
	info.data = data;
	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
}

int CWebBrowserContainer::InvokeDialogCallback(int type, char *title, char *text, char *msg, int *value)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaDialogCb_t	dlg;
	int ret;

	if (!moz->dialog_cb)
	    return NS_OK;

	cb = moz->dialog_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_DIALOG;
	cbinfo.cbdata = &dlg;

	memset(&dlg, 0, sizeof(PtMozillaDialogCb_t));
	dlg.type = type;
	dlg.title = title;
	dlg.text = text;
	if ((type == Pt_MOZ_DIALOG_ALERT_CHECK) || (type == Pt_MOZ_DIALOG_CONFIRM_CHECK))
		dlg.message = msg;

	ret = PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);
	if (value)
		*value = dlg.ret_value;
    return (ret);
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports implementation

NS_IMPL_ADDREF(CWebBrowserContainer)
NS_IMPL_RELEASE(CWebBrowserContainer)

NS_INTERFACE_MAP_BEGIN(CWebBrowserContainer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
	NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
	NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
	NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
	NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
	NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
	NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
	NS_INTERFACE_MAP_ENTRY(nsIDNSListener)
    NS_INTERFACE_MAP_ENTRY(nsIContextMenuListener)
    NS_INTERFACE_MAP_ENTRY(nsICommandHandler)
		NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END


///////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor

NS_IMETHODIMP CWebBrowserContainer::GetInterface(const nsIID & uuid, void * *result)
{
    return QueryInterface(uuid, result);
}


///////////////////////////////////////////////////////////////////////////////
// nsIContextMenuListener

NS_IMETHODIMP CWebBrowserContainer::OnShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode)
{	
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaContextCb_t	cmenu;

	if (!moz->context_cb)
	    return NS_OK;

	cb = moz->context_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_CONTEXT;
	cbinfo.cbdata = &cmenu;

	memset(&cmenu, 0, sizeof(PtMozillaContextCb_t));
	if (aContextFlags & CONTEXT_NONE)
		cmenu.flags |= Pt_MOZ_CONTEXT_NONE;
	if (aContextFlags & CONTEXT_LINK)
		cmenu.flags |= Pt_MOZ_CONTEXT_LINK;
	if (aContextFlags & CONTEXT_IMAGE)
		cmenu.flags |= Pt_MOZ_CONTEXT_IMAGE;
	if (aContextFlags & CONTEXT_DOCUMENT)
		cmenu.flags |= Pt_MOZ_CONTEXT_DOCUMENT;
	if (aContextFlags & CONTEXT_TEXT)
		cmenu.flags |= Pt_MOZ_CONTEXT_TEXT;
	if (aContextFlags & CONTEXT_INPUT)
		cmenu.flags |= Pt_MOZ_CONTEXT_INPUT;

	nsCOMPtr<nsIDOMMouseEvent> mouseEvent (do_QueryInterface( aEvent ));
	if(!mouseEvent) return NS_OK;
	mouseEvent->GetScreenX( &cmenu.x );
	mouseEvent->GetScreenY( &cmenu.y );

	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);


	/* store the url we clicked on */
	nsAutoString rightClickUrl;

	nsresult rv = NS_OK;
	nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(aNode, &rv));

	if(NS_FAILED(rv)) return NS_OK;

	// Note that this string is in UCS2 format
	rv = linkElement->GetHref( rightClickUrl );
	if(NS_FAILED(rv)) {
		if( moz->rightClickUrl ) free( moz->rightClickUrl );
		moz->rightClickUrl = NULL;
		return NS_OK;
		}

	if( moz->rightClickUrl ) free( moz->rightClickUrl );
	moz->rightClickUrl = ToNewCString(rightClickUrl);

	return NS_OK;
	}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

/* void onProgressChange (in nsIWebProgress aProgress, in nsIRequest aRequest, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP CWebBrowserContainer::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress)
{
	PtMozillaWidget_t 		*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 		*cb;
	PtCallbackInfo_t 		cbinfo;
	PtMozillaProgressCb_t   prog;

	if (!moz->progress_cb)
	    return NS_OK;

	cb = moz->progress_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_PROGRESS;
	cbinfo.cbdata = &prog;
	prog.cur = curTotalProgress;
	prog.max = maxTotalProgress;

	if( prog.cur > prog.max && prog.max != -1 && prog.max != 0 ) 
		prog.cur = prog.max; // Progress complete

    PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

    return NS_OK;
}

typedef enum
{
  GTK_MOZ_EMBED_FLAG_START = 1,
  GTK_MOZ_EMBED_FLAG_REDIRECTING = 2,
  GTK_MOZ_EMBED_FLAG_TRANSFERRING = 4,
  GTK_MOZ_EMBED_FLAG_NEGOTIATING = 8,
  GTK_MOZ_EMBED_FLAG_STOP = 16,

  GTK_MOZ_EMBED_FLAG_IS_REQUEST = 65536,
  GTK_MOZ_EMBED_FLAG_IS_DOCUMENT = 131072,
  GTK_MOZ_EMBED_FLAG_IS_NETWORK = 262144,
  GTK_MOZ_EMBED_FLAG_IS_WINDOW = 524288
} GtkMozEmbedProgressFlags;

/* These are from various networking headers */

typedef enum
{
  /* NS_ERROR_UNKNOWN_HOST */
  GTK_MOZ_EMBED_STATUS_FAILED_DNS     = 2152398878U,
 /* NS_ERROR_CONNECTION_REFUSED */
  GTK_MOZ_EMBED_STATUS_FAILED_CONNECT = 2152398861U,
 /* NS_ERROR_NET_TIMEOUT */
  GTK_MOZ_EMBED_STATUS_FAILED_TIMEOUT = 2152398862U,
 /* NS_BINDING_ABORTED */
  GTK_MOZ_EMBED_STATUS_FAILED_USERCANCELED = 2152398850U
} GtkMozEmbedStatusFlags;


/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest request, in long progressStateFlags, in unsinged long aStatus); */
NS_IMETHODIMP CWebBrowserContainer::OnStateChange(nsIWebProgress* aWebProgress, nsIRequest *aRequest, PRInt32 progressStateFlags, nsresult aStatus)
{
	PtMozillaWidget_t 		*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 		*cb = NULL;
	PtCallbackInfo_t 		cbinfo;
	PRUnichar *url;
	PtMozillaNetStateCb_t   state;

/* ATENTIE printf( "CWebBrowserContainer::OnStateChange progressStateFlags=0x%x aStatus=0x%x (%d %d)\n\n\n", progressStateFlags, aStatus,
progressStateFlags & STATE_IS_NETWORK, NS_FAILED( aStatus ) ); */

	if( mSkipOnState ) {
		if( moz->download_dest && !moz->MyBrowser->app_launcher ) {
			if( progressStateFlags == (STATE_IS_REQUEST|STATE_IS_BROKEN) ) {
				mDownloadDocument = 1;
				}
			else if( ( progressStateFlags & STATE_IS_NETWORK ) && ( progressStateFlags & STATE_STOP ) ) {
				/* a request for Pt_ARG_MOZ_DOWNLOAD is done by trying to call LoadURI(). If the nsUnknownContentHandler
				pops up, then moz->MyBrowser->app_launcher will be not null and we will use it to save the document ( old code ).
				If the page loaded successfully, then use the nsIWebBrowserPersist to save the document */
				if( mDownloadDocument ) {
					mSkipOnState = 0;
					mDownloadDocument = 0;
					}
				}
			}
		if( mSkipOnState ) return NS_OK;
		}
	
	aRequest->GetName( &url );
	nsString surl( url );

	if( ( progressStateFlags & STATE_IS_NETWORK ) && NS_FAILED( aStatus ) ) {
		PtWebErrorCallback_t cbw;

		/* invoke the Pt_CB_WEB_ERROR in the client */
		cb = moz->web_error_cb;
		memset(&cbinfo, 0, sizeof(cbinfo));
		cbinfo.reason = Pt_CB_MOZ_ERROR;
		cbinfo.cbdata = &cbw;

		memset( &cbw, 0, sizeof( PtWebErrorCallback_t ) );
		char *s = ToNewCString(surl);
		strcpy( cbw.url, s );
		free( s );


		cbw.type = WWW_ERROR_TOPVIEW;
		switch( aStatus ) {
			case NS_ERROR_UNKNOWN_HOST:				cbw.reason = -12; break;
			case NS_ERROR_NET_TIMEOUT: 				cbw.reason = -408; break;
			case NS_ERROR_NO_CONTENT:					cbw.reason = -8; break;
			case NS_ERROR_FILE_NOT_FOUND:			cbw.reason = -404; break;
			case NS_ERROR_CONNECTION_REFUSED:	cbw.reason = -13; break;

			/* these will not cause the web error */
			case NS_BINDING_ABORTED:					break;

			default:													cbw.reason = -1; break;
			}
		if( cbw.reason ) PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);
		/* let it check for STATE_STOP */
		}

	memset(&cbinfo, 0, sizeof(cbinfo));

	if( progressStateFlags & STATE_IS_NETWORK /* STATE_IS_REQUEST STATE_IS_DOCUMENT */ ) 
	{
    if( progressStateFlags & STATE_START ) {
			cbinfo.reason = Pt_CB_MOZ_START;
			if( ( cb = moz->start_cb ) )
				PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
    	}
    else if( progressStateFlags & STATE_STOP ) {
			PtWebCompleteCallback_t cbcomplete;

			/* if the mozilla was saving a file as a result of Pt_ARG_MOZ_DOWNLOAD or Pt_ARG_MOZ_UNKNOWN_RESP, move the temporary file into the desired destination ( moz->download_dest ) */
			if( moz->download_dest ) {
				if( moz->MyBrowser->app_launcher ) {
					nsCOMPtr<nsIURI> aSourceUrl;
					PRInt64 dummy;
					nsCOMPtr<nsIFile> tempFile;
					moz->MyBrowser->app_launcher->GetDownloadInfo( getter_AddRefs(aSourceUrl), &dummy, getter_AddRefs( tempFile ) );

					if( tempFile ) {
						nsresult rv;
						nsCOMPtr<nsILocalFile> fileToUse = do_CreateInstance( NS_LOCAL_FILE_CONTRACTID, &rv );
						fileToUse->InitWithPath( moz->download_dest );
	
						PRBool equalToTempFile = PR_FALSE;
						PRBool filetoUseAlreadyExists = PR_FALSE;
						fileToUse->Equals( tempFile, &equalToTempFile );
						fileToUse->Exists(&filetoUseAlreadyExists);
						if( filetoUseAlreadyExists && !equalToTempFile )
							fileToUse->Remove(PR_FALSE);

						// extract the new leaf name from the file location
						nsXPIDLCString fileName;
						fileToUse->GetLeafName(getter_Copies(fileName));
						nsCOMPtr<nsIFile> directoryLocation;
						fileToUse->GetParent(getter_AddRefs(directoryLocation));
						if( directoryLocation ) rv = tempFile->MoveTo(directoryLocation, fileName);
						}

					moz->MyBrowser->app_launcher = NULL;
					}
				else {
					MozSavePageAs( m_pOwner, moz->download_dest, Pt_MOZ_SAVEAS_HTML );
					}
				}

			cbinfo.reason = Pt_CB_MOZ_COMPLETE;
			cbinfo.cbdata = &cbcomplete;
			memset( &cbcomplete, 0, sizeof( PtWebCompleteCallback_t ) );
			char *s = ToNewCString(surl);
			strcpy( cbcomplete.url, s );
			free( s );

			if( ( cb = moz->complete_cb ) )
				PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);
    	}
	}

	// invoke the raw status callbacks for page load status
	cbinfo.reason = Pt_CB_MOZ_NET_STATE;
	cbinfo.cbdata = &state;
	state.flags = progressStateFlags;
	state.status = aStatus;
	state.url = (char *)ToNewCString(surl);
	char *statusMessage = "";
	PRInt32 flags = progressStateFlags;

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



/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP CWebBrowserContainer::OnLocationChange(nsIWebProgress* aWebProgress,
                                                     nsIRequest* aRequest,
                                                     nsIURI *location)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb = NULL;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaUrlCb_t    url;

	if (!moz->url_cb)
	    return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &url;
	cbinfo.reason = Pt_CB_MOZ_URL;
	cb = moz->url_cb;
	location->GetSpec(&(url.url));

	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

    return NS_OK;
}

NS_IMETHODIMP 
CWebBrowserContainer::OnStatusChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     nsresult aStatus,
                                     const PRUnichar* aMessage)
{
	PtMozillaWidget_t     *moz = (PtMozillaWidget_t *) m_pOwner;

	/* mozilla was saving a file as a result of Pt_ARG_MOZ_DOWNLOAD or Pt_ARG_MOZ_UNKNOWN_RESP */
	if( moz->MyBrowser->app_launcher && moz->download_dest ) {
		return Pt_CONTINUE;
		}

	if( aMessage ) {
		nsString pp(aMessage);
		char *message = ToNewCString(pp);
		InvokeInfoCallback( Pt_MOZ_INFO_CONNECT, (unsigned int) 0, message );
		delete [] message;
		}
	return NS_OK;
}

NS_IMETHODIMP 
CWebBrowserContainer::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                       nsIRequest *aRequest, 
                                       PRInt32 state)
{
	unsigned mState = 0;

	if (state & Pt_SSL_STATE_IS_INSECURE)
		mState |= Pt_SSL_STATE_IS_INSECURE;
	if (state & Pt_SSL_STATE_IS_BROKEN)
		mState |= Pt_SSL_STATE_IS_BROKEN;
	if (state & Pt_SSL_STATE_IS_SECURE)
		mState |= Pt_SSL_STATE_IS_SECURE;
	if (state & Pt_SSL_STATE_SECURE_HIGH)
		mState |= Pt_SSL_STATE_SECURE_HIGH;
	if (state & Pt_SSL_STATE_SECURE_MED)
		mState |= Pt_SSL_STATE_SECURE_MED;
	if (state & Pt_SSL_STATE_SECURE_LOW)
		mState |= Pt_SSL_STATE_SECURE_LOW;

	InvokeInfoCallback(Pt_MOZ_INFO_SSL, mState, nsnull);

    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDNSListener

/* void OnStartLookup (in nsISupports ctxt, in string hostname); */
NS_IMETHODIMP CWebBrowserContainer::OnStartLookup(nsISupports *ctxt, const char *hostname)
{	
	PtMozillaWidget_t       *moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t        *cb = NULL;
	PtCallbackInfo_t        cbinfo;
	PtMozillaNetStateCb_t   state;

	cbinfo.reason = Pt_CB_MOZ_NET_STATE;
	cbinfo.cbdata = &state;
	state.flags = 0;
	state.status = 0;
	state.url = (char *)hostname;
	char *statusMessage = "Resolving host name:";
	state.message = statusMessage;
	if( ( cb = moz->net_state_cb ) )
    	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

    return NS_OK;
}

/* [noscript] void OnFound (in nsISupports ctxt, in string hostname, in nsHostEntStar entry); */
NS_IMETHODIMP CWebBrowserContainer::OnFound(nsISupports *ctxt, const char *hostname, nsHostEnt * entry)
{
	PtMozillaWidget_t       *moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t        *cb = NULL;
	PtCallbackInfo_t        cbinfo;
	PtMozillaNetStateCb_t   state;

	cbinfo.reason = Pt_CB_MOZ_NET_STATE;
	cbinfo.cbdata = &state;
	state.flags = 0;
	state.status = 0;
	state.url = (char *)hostname;
	char *statusMessage = "Opening connection:";
	state.message = statusMessage;
	if( ( cb = moz->net_state_cb ) )
    	PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo);

    return NS_OK;
}

/* void OnStopLookup (in nsISupports ctxt, in string hostname, in nsresult status); */
NS_IMETHODIMP CWebBrowserContainer::OnStopLookup(nsISupports *ctxt, const char *hostname, nsresult status)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// nsIURIContentListener

/* void onStartURIOpen (in nsIURI aURI, out boolean aAbortOpen); */
NS_IMETHODIMP CWebBrowserContainer::OnStartURIOpen(nsIURI *pURI, PRBool *aAbortOpen)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb = NULL;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaUrlCb_t    url;

	if (!moz->open_cb)
	    return NS_OK;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &url;
	cbinfo.reason = Pt_CB_MOZ_OPEN;
	cb = moz->open_cb;
	pURI->GetSpec(&(url.url));

	if (PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo) == Pt_END)
	{
		*aAbortOpen = PR_TRUE;
		return NS_ERROR_ABORT;
	}

	*aAbortOpen = PR_FALSE;
	return NS_OK;
}


/* void doContent (in string aContentType, in boolean aIsContentPreferred, in nsIRequest aOpenedChannel, out nsIStreamListener aContentHandler, out boolean aAbortProcess); */
NS_IMETHODIMP CWebBrowserContainer::DoContent(const char *aContentType, PRBool aIsContentPreferred, nsIRequest *aOpenedChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* boolean isPreferred (in string aContentType, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::IsPreferred(const char *aContentType, char **aDesiredContentType, PRBool *aCanHandleContent)
{

  if (aContentType &&
      (!strcasecmp(aContentType, "text/html")   ||
       !strcasecmp(aContentType, "text/plain")  ||
       !strcasecmp(aContentType, "application/vnd.mozilla.xul+xml")    ||
       !strcasecmp(aContentType, "text/rdf")    ||
       !strcasecmp(aContentType, "text/xml")    ||
       !strcasecmp(aContentType, "text/css")    ||
       !strcasecmp(aContentType, "image/gif")   ||
       !strcasecmp(aContentType, "image/jpeg")  ||
       !strcasecmp(aContentType, "image/png")   ||
       !strcasecmp(aContentType, "image/tiff")  ||
       !strcasecmp(aContentType, "application/http-index-format"))) {
    *aCanHandleContent = PR_TRUE;
  }
  else {
    *aCanHandleContent = PR_FALSE;
  }

  return NS_OK;
}


/* boolean canHandleContent (in string aContentType, in boolean aIsContentPreferred, out string aDesiredContentType); */
NS_IMETHODIMP CWebBrowserContainer::CanHandleContent(const char *aContentType, PRBool aIsContentPreferred, char **aDesiredContentType, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsISupports loadCookie; */
NS_IMETHODIMP CWebBrowserContainer::GetLoadCookie(nsISupports * *aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebBrowserContainer::SetLoadCookie(nsISupports * aLoadCookie)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* attribute nsIURIContentListener parentContentListener; */
NS_IMETHODIMP CWebBrowserContainer::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP CWebBrowserContainer::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsIDocShellTreeOwner


NS_IMETHODIMP
CWebBrowserContainer::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
	return NS_ERROR_FAILURE;
/*
	nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(m_pOwner->mWebBrowser));
	NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
	return docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aFoundItem);
	*/
}

static nsIDocShellTreeItem* contentShell = nsnull; 
NS_IMETHODIMP
CWebBrowserContainer::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
    if (aPrimary)
        contentShell = aContentShell;
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
	// NS_ERROR("Haven't Implemented this yet");
    NS_IF_ADDREF(contentShell);
	*aShell = contentShell;
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaNewAreaCb_t	resize;

	if (!moz->resize_cb)
	    return NS_OK;

	cb = moz->resize_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_NEW_AREA;
	cbinfo.cbdata = &resize;

	memset(&resize, 0, sizeof(PtMozillaNewAreaCb_t));
	resize.flags = Pt_MOZ_NEW_AREA_SET_SIZE;
	resize.area.size.w = aCX;
	resize.area.size.h = aCY;

	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Data streaming interface

static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);

NS_IMETHODIMP CWebBrowserContainer::OpenStream( nsIWebBrowser *webBrowser, const char *aBaseURI, const char *aContentType )
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  NS_ENSURE_ARG_POINTER(aContentType);

  nsresult rv = NS_OK;
  PhMozEmbedStream *newStream = nsnull;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsIContentViewerContainer> viewerContainer;
  nsCOMPtr<nsIContentViewer> contentViewer;
	nsCOMPtr<nsILoadGroup> loadGroup;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString docLoaderContractID;
  nsCAutoString spec(aBaseURI);
  
  // check to see if we need to close the current stream
  if (mDoingStream)
    CloseStream();
  mDoingStream = PR_TRUE;
  // create our new stream object
  newStream = new PhMozEmbedStream();
  if (!newStream)
    return NS_ERROR_OUT_OF_MEMORY;
  // we own this
  NS_ADDREF(newStream);
  // initialize it
  newStream->Init();
  // QI it to the right interface
  mStream = do_QueryInterface(newStream);
  if (!mStream)
    return NS_ERROR_FAILURE;
  // ok, now we're just using it for an nsIInputStream interface so
  // release our second reference to it.
  NS_RELEASE(newStream);
  
  // get our hands on the primary content area of that docshell

  // QI that back to a docshell
	docShell = do_GetInterface( webBrowser );
  if (!docShell)
    return NS_ERROR_FAILURE;

  // QI that to a content viewer container
  viewerContainer = do_QueryInterface(docShell);
  if (!viewerContainer)
    return NS_ERROR_FAILURE;

  // create a new uri object
  uri = do_CreateInstance(kSimpleURICID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = uri->SetSpec(spec.get());
  if (NS_FAILED(rv))
    return rv;

  // create a new load group
  rv = NS_NewLoadGroup(getter_AddRefs(loadGroup), nsnull);
  if (NS_FAILED(rv))
    return rv;
  
  // create a new input stream channel
  rv = NS_NewInputStreamChannel(getter_AddRefs(mChannel), uri, mStream, aContentType,
				1024 /* len */);
  if (NS_FAILED(rv))
    return rv;

  // set the channel's load group
  rv = mChannel->SetLoadGroup(loadGroup);
  if (NS_FAILED(rv))
    return rv;

  // find a document loader for this command plus content type
  // combination

  docLoaderContractID  = NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX;
  docLoaderContractID += "view;1?type=";
  docLoaderContractID += aContentType;

  docLoaderFactory = do_CreateInstance(docLoaderContractID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // ok, create an instance of the content viewer for that command and
  // mime type
  rv = docLoaderFactory->CreateInstance("view",
					mChannel,
					loadGroup,
					aContentType,
					viewerContainer,
					nsnull,
					getter_AddRefs(mStreamListener),
					getter_AddRefs(contentViewer));

  if (NS_FAILED(rv))
    return rv;

  // set the container viewer container for this content view
  rv = contentViewer->SetContainer(viewerContainer);
  if (NS_FAILED(rv))
    return rv;

  // embed this sucker.
  rv = viewerContainer->Embed(contentViewer, "view", nsnull);
  if (NS_FAILED(rv))
    return rv;

  // start our request
  rv = mStreamListener->OnStartRequest(mChannel, NULL);
  if (NS_FAILED(rv))
    return rv;
  
  return NS_OK;
}

NS_IMETHODIMP CWebBrowserContainer::AppendToStream (const char *aData, int32 aLen)
{
  nsIInputStream *inputStream;
  inputStream = mStream.get();
  PhMozEmbedStream *embedStream = (PhMozEmbedStream *)inputStream;
  nsresult rv;
  NS_ENSURE_STATE(mDoingStream);
  rv = embedStream->Append(aData, aLen);
  if (NS_FAILED(rv))
    return rv;
  rv = mStreamListener->OnDataAvailable(mChannel,
					NULL,
					mStream,
					mOffset, /* offset */
					aLen); /* len */
  mOffset += aLen;
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;
}

NS_IMETHODIMP CWebBrowserContainer::CloseStream (void)
{
  nsresult rv;
  NS_ENSURE_STATE(mDoingStream);
  mDoingStream = PR_FALSE;
  rv = mStreamListener->OnStopRequest( mChannel, NULL, NS_OK );
  if (NS_FAILED(rv))
    return rv;
  mStream = nsnull;
  mChannel = nsnull;
  mStreamListener = nsnull;
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP CWebBrowserContainer::IsStreaming( ) {
	return mDoingStream;
	}


//*****************************************************************************
// WebBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

/* void setDimensions (in unsigned long flags, in long x, in long y, in long cx, in long cy); */
NS_IMETHODIMP CWebBrowserContainer::SetDimensions(PRUint32 flags, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaNewAreaCb_t	resize;

	if (!moz->resize_cb)
	    return NS_OK;

	cb = moz->resize_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_NEW_AREA;
	cbinfo.cbdata = &resize;

	memset(&resize, 0, sizeof(PtMozillaNewAreaCb_t));

	if( cx==0 && cy==0 )
		resize.flags = Pt_MOZ_NEW_AREA_SET_POSITION;
	else resize.flags = Pt_MOZ_NEW_AREA_SET_AREA;

	resize.area.pos.x = x;
	resize.area.pos.y = y;
	resize.area.size.w = cx;
	resize.area.size.h = cy;

	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

	return NS_OK;
}

/* void getDimensions (in unsigned long flags, out long x, out long y, out long cx, out long cy); */
NS_IMETHODIMP CWebBrowserContainer::GetDimensions(PRUint32 flags, PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setFocus (); */
NS_IMETHODIMP CWebBrowserContainer::SetFocus()
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) m_pOwner;
	nsCOMPtr<nsIBaseWindow> browserBaseWindow = do_QueryInterface( moz->MyBrowser->WebBrowser );
/* ATENTIE */ printf( "CWebBrowserContainer::SetFocus\n" );
	return browserBaseWindow->SetFocus();
}

/* attribute boolean visibility; */
NS_IMETHODIMP CWebBrowserContainer::GetVisibility(PRBool *aVisibility)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP CWebBrowserContainer::SetVisibility(PRBool aVisibility)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring title; */
NS_IMETHODIMP CWebBrowserContainer::GetTitle(PRUnichar * *aTitle)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP CWebBrowserContainer::SetTitle(const PRUnichar * aTitle) {
	nsString mTitleString(aTitle);
	InvokeInfoCallback(Pt_MOZ_INFO_TITLE, (unsigned int) 0, ToNewCString(mTitleString));
	return NS_OK;
}

/* [noscript] readonly attribute voidPtr siteWindow; */
NS_IMETHODIMP CWebBrowserContainer::GetSiteWindow(void * *aSiteWindow)
{

	*aSiteWindow = this;
	return NS_OK;
}

NS_IMETHODIMP
CWebBrowserContainer::DestroyBrowserWindow(void)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;

	if (!moz->destroy_cb)
	    return NS_OK;

	cb = moz->destroy_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_DESTROY;

	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);

	return NS_OK;
}




///////////////////////////////////////////////////////////////////////////////
// nsIWebBrowserChrome implementation

NS_IMETHODIMP
CWebBrowserContainer::SetStatus(PRUint32 statusType, const PRUnichar *status)
{
	nsString 	mStatus(status);
	int 		type = 0;

	switch (statusType)
	{
		case STATUS_SCRIPT:
			type = Pt_MOZ_INFO_JSSTATUS;
			break;
		case STATUS_SCRIPT_DEFAULT:
			break;
		case STATUS_LINK:
			type = Pt_MOZ_INFO_LINK;
			break;
		default:
			break;
	}


	if (type != 0)
		InvokeInfoCallback(type, (unsigned int) 0, ToNewCString(mStatus));

	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::GetWebBrowser(nsIWebBrowser * *aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SetWebBrowser(nsIWebBrowser * aWebBrowser)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::GetChromeFlags(PRUint32 *aChromeFlags)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SetChromeFlags(PRUint32 aChromeFlags)
{
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
CWebBrowserContainer::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
/* ATENTIE */ printf( "SizeBrowserTo!!!!!!!!!!!\n\n\n" );
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebBrowserContainer::IsWindowModal(PRBool *_retval)
{
	// we're not
	*_retval = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::ShowAsModal(void)
{
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CWebBrowserContainer::ExitModalEventLoop(nsresult aStatus)
{
	// Ignore request to exit modal loop
	return NS_OK;
}

NS_IMETHODIMP
CWebBrowserContainer::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                     PRBool aPersistCX)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CWebBrowserContainer::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                     PRBool* aPersistCX)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver implementation


NS_IMETHODIMP
CWebBrowserContainer::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
/* ATENTIE */ printf("!!!!!!!!!!!!! OnStartRequest\n\n\n" );
	return NS_OK;
}


NS_IMETHODIMP
CWebBrowserContainer::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode)
{
/* ATENTIE */ printf("!!!!!!!!!!!!! OnStopRequest\n\n\n" );
	return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsICommandHandler implementation

/* void do (in string aCommand, in string aStatus); */
NS_IMETHODIMP CWebBrowserContainer::Exec(const char *aCommand, const char *aStatus, char **aResult)
{
    return NS_OK;
}

/* void query (in string aCommand, in string aStatus); */
NS_IMETHODIMP CWebBrowserContainer::Query(const char *aCommand, const char *aStatus, char **aResult)
{
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPrintListener implementation

void CWebBrowserContainer::InvokePrintCallback(int status, unsigned int cur, unsigned int max)
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) m_pOwner;
	PtCallbackList_t 	*cb;
	PtCallbackInfo_t 	cbinfo;
	PtMozillaPrintStatusCb_t	pstatus;

	if (!moz->print_status_cb)
	    return;

	cb = moz->print_status_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_PRINT_STATUS;
	cbinfo.cbdata = &pstatus;

	memset(&pstatus, 0, sizeof(PtMozillaPrintStatusCb_t));
	pstatus.status = status;
	pstatus.max = max;
	pstatus.cur = cur;
	PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo);
}

/* void OnStartPrinting (); */
NS_IMETHODIMP CWebBrowserContainer::OnStartPrinting()
{
	InvokePrintCallback(Pt_MOZ_PRINT_START, 0, 0);
    return NS_OK;
}

/* void OnProgressPrinting (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP CWebBrowserContainer::OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax)
{
	InvokePrintCallback(Pt_MOZ_PRINT_PROGRESS, aProgress, aProgressMax);
    return NS_OK;
}

/* void OnEndPrinting (in PRUint32 aStatus); */
NS_IMETHODIMP CWebBrowserContainer::OnEndPrinting(PRUint32 aStatus)
{
	InvokePrintCallback(Pt_MOZ_PRINT_COMPLETE, 0, 0);
    return NS_OK;
}

/* see the gtk version */
nsresult CWebBrowserContainer::GetPIDOMWindow( nsPIDOMWindow **aPIWin ) {
  PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) m_pOwner;
  *aPIWin = nsnull;

  // get the content DOM window for that web browser
  nsCOMPtr<nsIDOMWindow> domWindow;
  moz->MyBrowser->WebBrowser->GetContentDOMWindow( getter_AddRefs( domWindow ) );
  if( !domWindow ) return NS_ERROR_FAILURE;

  // get the private DOM window
  nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
  // and the root window for that DOM window
  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  domWindowPrivate->GetPrivateRoot(getter_AddRefs(rootWindow));

  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));

  *aPIWin = piWin.get();

  if (*aPIWin) {
    NS_ADDREF(*aPIWin);
    return NS_OK;
    }

  return NS_ERROR_FAILURE;
  }

// nsIWebBrowserChromeFocus
NS_IMETHODIMP CWebBrowserContainer::FocusNextElement() {
  return NS_ERROR_NOT_IMPLEMENTED;
  }
NS_IMETHODIMP CWebBrowserContainer::FocusPrevElement() {
  return NS_ERROR_NOT_IMPLEMENTED;
  }



// ---------------------------------------------------------------------------
//	Window Creator
// ---------------------------------------------------------------------------

class CWindowCreator : public nsIWindowCreator
{
  public:
                         CWindowCreator();
    virtual             ~CWindowCreator();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWINDOWCREATOR 
};

NS_IMPL_ISUPPORTS1(CWindowCreator, nsIWindowCreator);

CWindowCreator::CWindowCreator()
{
    NS_INIT_ISUPPORTS();
}

CWindowCreator::~CWindowCreator()
{
}

NS_IMETHODIMP CWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                              PRUint32 aChromeFlags,
                                              nsIWebBrowserChrome **_retval)
{
	return NS_OK;
}


/*
   InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This will be used by Gecko C++ code
   (never JS) to create new windows when no previous window is handy
   to begin with. This is done in a few exceptional cases, like PSM code.
   Failure to set this callback will only disable the ability to create
   new windows under these circumstances.
*/

nsresult InitializeWindowCreator()
{
	// Create a CWindowCreator and give it to the WindowWatcher service
	// The WindowWatcher service will own it so we don't keep a ref.
	CWindowCreator *windowCreator = new CWindowCreator;
	if (!windowCreator) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	if (!wwatch) return NS_ERROR_FAILURE;
	return wwatch->SetWindowCreator(windowCreator);
}

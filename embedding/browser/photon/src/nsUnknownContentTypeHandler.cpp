/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsUnknownContentTypeHandler.h"
#include "nsIGenericFactory.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShell.h"
#include "mimetype/nsIMIMEInfo.h"
#include "nsIURI.h"
#include "nsILocalFile.h"

#include "EmbedPrivate.h"
#include "PtMozilla.h"

#include <photon/PtWebClient.h>

nsUnknownContentTypeHandler::nsUnknownContentTypeHandler( ) {
	NS_INIT_ISUPPORTS();
///* ATENTIE */ printf( "In nsUnknownContentTypeHandler constructor\n" );
	}

nsUnknownContentTypeHandler::~nsUnknownContentTypeHandler( )
{
///* ATENTIE */ printf( "In destructor mURL=%s\n", mURL );

	/* remove this download from our list */
	RemoveDownload( mMozillaWidget, mDownloadTicket );

	if( mURL ) free( mURL );
}


NS_IMETHODIMP nsUnknownContentTypeHandler::Show( nsIHelperAppLauncher *aLauncher, nsISupports *aContext, PRBool aForced )
{
	return aLauncher->SaveToDisk( nsnull, PR_FALSE );
}

NS_IMETHODIMP nsUnknownContentTypeHandler::PromptForSaveToFile( nsIHelperAppLauncher* aLauncher,
                                                                nsISupports *aWindowContext,
                                                                const PRUnichar *aDefaultFile,
                                                                const PRUnichar *aSuggestedFileExtension,
                                                                nsILocalFile **_retval )
{
///* ATENTIE */ printf("PromptForSaveToFile!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");
	NS_ENSURE_ARG_POINTER(_retval);
	*_retval = nsnull;

	aLauncher->SetWebProgressListener( this );

	/* try to get the PtMozillawidget_t* pointer form the aContext - use the fact the the WebBrowserContainer is
		registering itself as nsIDocumentLoaderObserver ( SetDocLoaderObserver ) */
	nsCOMPtr<nsIDOMWindow> domw( do_GetInterface( aWindowContext ) );
	nsIDOMWindow *parent;
	domw->GetParent( &parent );
	PtWidget_t *w = GetWebBrowser( parent );
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) w;

	mMozillaWidget = moz;

	/* get the suggested filename */
	NS_ConvertUCS2toUTF8 theUnicodeString( aDefaultFile );
	const char *filename = theUnicodeString.get( );

	/* get the url */
	nsCOMPtr<nsIURI> aSourceUrl;
	aLauncher->GetSource( getter_AddRefs(aSourceUrl) );
	char *url;
	nsCAutoString specString;
	aSourceUrl->GetSpec(specString);
	url = (char *) specString.get();

	mURL = strdup( url );

	/* get the mime type */
	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	aLauncher->GetMIMEInfo( getter_AddRefs(mimeInfo) );
	char *mimeType;
	mimeInfo->GetMIMEType( &mimeType );


	PtCallbackInfo_t cbinfo;
	PtWebUnknownWithNameCallback_t cb;

	memset( &cbinfo, 0, sizeof( cbinfo ) );
	cbinfo.reason = Pt_CB_MOZ_UNKNOWN;
	cbinfo.cbdata = &cb;
	cb.action = Pt_WEB_ACTION_OK;
	cb.content_type = mimeType;
	cb.url = url;
	cb.content_length = strlen( cb.url );
	cb.suggested_filename = (char*)filename;
	PtInvokeCallbackList( moz->web_unknown_cb, (PtWidget_t *)moz, &cbinfo );
	/* this will modal wait for a Pt_ARG_WEB_UNKNOWN_RESP, in mozserver */

	/* we have the result in moz->moz_unknown_ctrl */
	if( moz->moz_unknown_ctrl->response != Pt_WEB_RESPONSE_OK ) return NS_ERROR_ABORT;

	mDownloadTicket = moz->moz_unknown_ctrl->download_ticket;

	/* the user chosen filename is moz->moz_unknown_ctrl->filename */
	nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
	NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

	nsCString s ( moz->moz_unknown_ctrl->filename );
	file->InitWithNativePath( s );
	if( !file ) return NS_ERROR_FAILURE;

	*_retval = file;
	NS_ADDREF( *_retval );

	/* add this download to our list */
	AddDownload( mMozillaWidget, mDownloadTicket, aLauncher, this );

	return NS_OK;
}


PtWidget_t *nsUnknownContentTypeHandler::GetWebBrowser(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  PtWidget_t *val = 0;

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  if (!wwatch) return nsnull;

  if( wwatch ) {
    nsCOMPtr<nsIDOMWindow> fosterParent;
    if (!aWindow) { // it will be a dependent window. try to find a foster parent.
      wwatch->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    wwatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }

  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      site->GetSiteWindow(reinterpret_cast<void **>(&val));
    }
  }

  return val;
}


/* nsIWebProgressListener interface */

/* void onProgressChange (in nsIWebProgress aProgress, in nsIRequest aRequest, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP nsUnknownContentTypeHandler::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress) {

///* ATENTIE */ printf("this=%p OnProgressChange curSelfProgress=%d maxSelfProgress=%d curTotalProgress=%d maxTotalProgress=%d\n",
//this, curSelfProgress, maxSelfProgress, curTotalProgress, maxTotalProgress );

	ReportDownload( Pt_WEB_DOWNLOAD_PROGRESS, curSelfProgress, maxSelfProgress, "" );

	return NS_OK;
	}

NS_IMETHODIMP nsUnknownContentTypeHandler::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus) {
	if( aStateFlags & STATE_STOP ) {
		ReportDownload( Pt_WEB_DOWNLOAD_DONE, 0, 0, "" );
		}
	return NS_OK;
	}

NS_IMETHODIMP nsUnknownContentTypeHandler::OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsIURI *location) {
	return NS_OK;
	}
NS_IMETHODIMP nsUnknownContentTypeHandler::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsresult aStatus, const PRUnichar* aMessage) {
	return NS_OK;
	}
NS_IMETHODIMP nsUnknownContentTypeHandler::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state) {
	return NS_OK;
	}

NS_IMETHODIMP nsUnknownContentTypeHandler::ReportDownload( int type, int current, int total, char *message )
{
  PtCallbackInfo_t cbinfo;
  PtWebDownloadCallback_t cb;

  memset( &cbinfo, 0, sizeof( cbinfo ) );
  cbinfo.reason = Pt_CB_MOZ_DOWNLOAD;
  cbinfo.cbdata = &cb;

  cb.download_ticket = mDownloadTicket;
  cb.type = type;
  cb.url = mURL;
  cb.current = current;
  cb.total = total;
  cb.message = message;

  PtInvokeCallbackList( mMozillaWidget->web_download_cb, (PtWidget_t *)mMozillaWidget, &cbinfo );
	return NS_OK;
  }

void AddDownload( PtMozillaWidget_t *moz, int download_ticket, nsIHelperAppLauncher* aLauncher, nsUnknownContentTypeHandler *unknown )
{
	Download_t *d;

	d = ( Download_t * ) calloc( 1, sizeof( Download_t ) );
	if( !d ) return;

	d->download_ticket = download_ticket;
	d->mLauncher = aLauncher;
	NS_RELEASE( aLauncher );

	d->unknown = unknown;

	/* insert d into moz->fDownload */
	moz->fDownload = ( Download_t ** ) realloc( moz->fDownload, ( moz->fDownloadCount + 1 ) * sizeof( Download_t * ) );
	if( !moz->fDownload ) return;

	moz->fDownload[ moz->fDownloadCount ] = d;
	moz->fDownloadCount++;
}

void RemoveDownload( PtMozillaWidget_t *moz, int download_ticket )
{
	int i;

  /* remove d from the moz->fDownload */
  for( i=0; i<moz->fDownloadCount; i++ ) {
    if( moz->fDownload[i]->download_ticket == download_ticket ) break;
    }

  if( i<moz->fDownloadCount ) {
    int j;
		Download_t *d;

		d = moz->fDownload[i];

    for( j=i; j<moz->fDownloadCount-1; j++ )
      moz->fDownload[j] = moz->fDownload[j+1];

    moz->fDownloadCount--;
    if( !moz->fDownloadCount ) {
      free( moz->fDownload );
      moz->fDownload = NULL;
      }

		d->unknown->ReportDownload( Pt_WEB_DOWNLOAD_CANCEL, 0, 0, "" );

  	free( d );
    }

///* ATENTIE */ printf( "after remove fDownloadCount=%d\n", moz->fDownloadCount );
}

Download_t *FindDownload( PtMozillaWidget_t *moz, int download_ticket )
{
	int i;
  for( i=0; i<moz->fDownloadCount; i++ ) {
    if( moz->fDownload[i]->download_ticket == download_ticket ) return moz->fDownload[i];
    }
	return NULL;
}

//#######################################################################################

#define className             nsUnknownContentTypeHandler
#define interfaceName         nsIHelperAppLauncherDialog
#define contractId            NS_IUNKNOWNCONTENTTYPEHANDLER_CONTRACTID

/* Component's implementation of Initialize. */
/* nsISupports Implementation for the class */
NS_IMPL_ADDREF( className );  
NS_IMPL_RELEASE( className )


/* QueryInterface implementation for this class. */
NS_IMETHODIMP className::QueryInterface( REFNSIID anIID, void **anInstancePtr ) { 
	nsresult rv = NS_OK; 
///* ATENTIE */ printf("QueryInterface!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");

	/* Check for place to return result. */
	if( !anInstancePtr ) rv = NS_ERROR_NULL_POINTER;
	else { 
		*anInstancePtr = 0; 
		if ( anIID.Equals( NS_GET_IID(nsIHelperAppLauncherDialog) ) ) { 
			*anInstancePtr = (void*) (nsIHelperAppLauncherDialog*)this; 
			NS_ADDREF_THIS();
			}
		else if ( anIID.Equals( NS_GET_IID(nsISupports) ) ) { 
			*anInstancePtr = (void*) ( (nsISupports*) (interfaceName*)this ); 
			NS_ADDREF_THIS();
			}
		else rv = NS_NOINTERFACE;
		} 
	return rv; 
	}

NS_GENERIC_FACTORY_CONSTRUCTOR( nsUnknownContentTypeHandler )

// The list of components we register
static nsModuleComponentInfo info[] = {
		{
			"nsUnknownContentTypeHandler",
			NS_IHELPERAPPLAUNCHERDIALOG_IID,
			NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
			nsUnknownContentTypeHandlerConstructor
		}
	};

int Init_nsUnknownContentTypeHandler_Factory( ) {
	nsresult rv;

	// create a generic factory for UnkHandler
	nsCOMPtr<nsIGenericFactory> factory;
	rv = NS_NewGenericFactory( getter_AddRefs(factory), info );
	if (rv != NS_OK)
		return rv;

	// register this factory with the component manager.
	rv = nsComponentManager::RegisterFactory( kCID, "nsUnknownContentTypeHandler", NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, factory, PR_TRUE);
	return NS_OK;
	}

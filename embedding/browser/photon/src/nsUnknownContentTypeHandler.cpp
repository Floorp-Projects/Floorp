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
#include "nsIMIMEInfo.h"
#include "nsIURI.h"
#include "nsIFile.h"

#include "WebBrowserContainer.h"
#include "PtMozilla.h"

#include <photon/PtWebClient.h>

nsUnknownContentTypeHandler::nsUnknownContentTypeHandler( ) {
	NS_INIT_REFCNT();
	}

nsUnknownContentTypeHandler::~nsUnknownContentTypeHandler( ) { }


NS_IMETHODIMP nsUnknownContentTypeHandler::ShowProgressDialog(nsIHelperAppLauncher *aLauncher, nsISupports *aContext ) {
	nsresult rv = NS_OK;

/* ATENTIE */ printf("ShowProgressDialog!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");

	/* we need a dummy listener because the nsExternalAppHandler class verifies that a progress window has been displayed */
	nsCOMPtr<nsIWebProgressListener> dummy = new nsWebProgressListener;
	aLauncher->SetWebProgressListener( dummy );

	return rv;
	}

NS_IMETHODIMP nsUnknownContentTypeHandler::Show( nsIHelperAppLauncher *aLauncher, nsISupports *aContext ) {
	nsresult rv = NS_OK;
/* ATENTIE */ printf("Show!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");

	/* try to get the PtMozillawidget_t* pointer form the aContext - use the fact the the WebBrowserContainer is
		registering itself as nsIDocumentLoaderObserver ( SetDocLoaderObserver ) */

	nsCOMPtr<nsIDOMWindow> domw( do_GetInterface( aContext ) );
	nsIDOMWindow *parent;
	domw->GetParent( &parent );
	CWebBrowserContainer *w = GetWebBrowser( parent );
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) w->m_pOwner;

	/* go ahead and start the downloading process */
	nsCOMPtr<nsIWebProgressListener> listener = NS_STATIC_CAST(nsIWebProgressListener*, moz->MyBrowser->WebBrowserContainer );
	aLauncher->SetWebProgressListener( listener );
	moz->MyBrowser->WebBrowserContainer->mSkipOnState = 0; /* reinstate nsIWebProgressListener's CWebBrowserContainer::OnStateChange() */

	/* get the mime type - need to provide it in the callback info */
	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	aLauncher->GetMIMEInfo( getter_AddRefs(mimeInfo) );
	char *mimeType;
	mimeInfo->GetMIMEType( &mimeType );

	nsCOMPtr<nsIURI> aSourceUrl;
	PRInt64 dummy;
	nsCOMPtr<nsIFile> aFile;
	aLauncher->GetDownloadInfo( getter_AddRefs(aSourceUrl), &dummy, getter_AddRefs(aFile) );
	char *url;
	aSourceUrl->GetSpec( &url );

	PtCallbackInfo_t cbinfo;
	PtWebUnknownCallback_t cb;

	memset( &cbinfo, 0, sizeof( cbinfo ) );
	cbinfo.reason = Pt_CB_MOZ_UNKNOWN;
	cbinfo.cbdata = &cb;
	cb.action = WWW_ACTION_OK;

	/* pass extra information to the mozilla widget, so that it will know what to do when Pt_ARG_MOZ_UNKNOWN_RESP comes */
	moz->MyBrowser->app_launcher = aLauncher;
	moz->MyBrowser->context = aContext;

	strcpy( cb.content_type, mimeType );
	strcpy( cb.url, url );
	cb.content_length = strlen( cb.url );
	PtInvokeCallbackList( moz->web_unknown_cb, (PtWidget_t *)moz, &cbinfo);
	return rv;
	}

/* only Show() method is used - remove this code */
NS_IMETHODIMP nsUnknownContentTypeHandler::PromptForSaveToFile( nsISupports * aWindowContext, const PRUnichar * aDefaultFile, const PRUnichar * aSuggestedFileExtension, nsILocalFile ** aNewFile ) {
/* ATENTIE */ printf("PromptForSaveToFile!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");
	return NS_OK;
	}


CWebBrowserContainer *nsUnknownContentTypeHandler::GetWebBrowser(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  CWebBrowserContainer *val = 0;

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


//###########################################################################
NS_IMPL_ISUPPORTS2(nsWebProgressListener, nsIWebProgressListener, nsISupportsWeakReference);

nsWebProgressListener::nsWebProgressListener() {
  NS_INIT_ISUPPORTS();
	}

nsWebProgressListener::~nsWebProgressListener() { }


/* nsIWebProgressListener interface */
/* void onProgressChange (in nsIWebProgress aProgress, in nsIRequest aRequest, in long curSelfProgress, in long maxSelfProgress, in long curTotalProgress, in long maxTotalProgress); */
NS_IMETHODIMP nsWebProgressListener::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress) {

/* ATENTIE */ printf("OnProgressChange curSelfProgress=%d maxSelfProgress=%d curTotalProgress=%d maxTotalProgress=%d\n",
curSelfProgress, maxSelfProgress, curTotalProgress, maxTotalProgress );

	return NS_OK;
	}
NS_IMETHODIMP nsWebProgressListener::OnStateChange(nsIWebProgress* aWebProgress, nsIRequest *aRequest, PRInt32 progressStateFlags, nsresult aStatus) {
	return NS_OK;
	}
NS_IMETHODIMP nsWebProgressListener::OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsIURI *location) {
	return NS_OK;
	}
NS_IMETHODIMP nsWebProgressListener::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsresult aStatus, const PRUnichar* aMessage) {
	return NS_OK;
	}
NS_IMETHODIMP nsWebProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state) {
	return NS_OK;
	}

//#######################################################################################

#define className             nsUnknownContentTypeHandler
#define interfaceName         nsIHelperAppLauncherDialog
#define contractId            NS_IUNKNOWNCONTENTTYPEHANDLER_CONTRACTID

/* Component's implementation of Initialize. */
/* nsISupports Implementation for the class */
NS_IMPL_ADDREF( className );  
NS_IMPL_RELEASE( className );


/* QueryInterface implementation for this class. */
NS_IMETHODIMP className::QueryInterface( REFNSIID anIID, void **anInstancePtr ) { 
	nsresult rv = NS_OK; 
/* ATENTIE */ printf("QueryInterface!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");

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
	"nsUnknownContentTypeHandler",
	NS_IHELPERAPPLAUNCHERDIALOG_IID,
	NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
	nsUnknownContentTypeHandlerConstructor
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

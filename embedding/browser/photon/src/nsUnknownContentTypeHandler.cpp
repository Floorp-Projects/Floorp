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

nsUnknownContentTypeHandler::nsUnknownContentTypeHandler( ) {
	NS_INIT_ISUPPORTS();
///* ATENTIE */ printf( "In nsUnknownContentTypeHandler constructor\n" );
	}

nsUnknownContentTypeHandler::~nsUnknownContentTypeHandler( )
{
///* ATENTIE */ printf( "In nsUnknownContentTypeHandler destr\n" );
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

	/* try to get the PtMozillawidget_t* pointer form the aContext - use the fact the the WebBrowserContainer is
		registering itself as nsIDocumentLoaderObserver ( SetDocLoaderObserver ) */
	nsCOMPtr<nsIDOMWindow> domw( do_GetInterface( aWindowContext ) );
	nsIDOMWindow *parent;
	domw->GetParent( &parent );
	PtWidget_t *w = GetWebBrowser( parent );
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) w;

	/* get the suggested filename */
	NS_ConvertUCS2toUTF8 theUnicodeString( aDefaultFile );
	const char *filename = theUnicodeString.get( );

	/* get the url */
	nsCOMPtr<nsIURI> aSourceUrl;
	aLauncher->GetSource( getter_AddRefs(aSourceUrl) );
	const char *url;
	nsCAutoString specString;
	aSourceUrl->GetSpec(specString);
	url = specString.get();

	/* get the mime type */
	nsCOMPtr<nsIMIMEInfo> mimeInfo;
	aLauncher->GetMIMEInfo( getter_AddRefs(mimeInfo) );
	nsCAutoString mimeType;
	mimeInfo->GetMIMEType( mimeType );

	PtCallbackInfo_t cbinfo;
	PtWebUnknownWithNameCallback_t cb;

	memset( &cbinfo, 0, sizeof( cbinfo ) );
	cbinfo.reason = Pt_CB_MOZ_UNKNOWN;
	cbinfo.cbdata = &cb;
	cb.action = Pt_WEB_ACTION_OK;
	cb.content_type = (char*)mimeType.get();
	cb.url = (char *)url;
	cb.content_length = strlen( cb.url );
	cb.suggested_filename = (char*)filename;
	PtInvokeCallbackList( moz->web_unknown_cb, (PtWidget_t *)moz, &cbinfo );
	/* this will modal wait for a Pt_ARG_WEB_UNKNOWN_RESP, in mozserver */

	/* we have the result in moz->moz_unknown_ctrl */
	if( moz->moz_unknown_ctrl->response != Pt_WEB_RESPONSE_OK ) return NS_ERROR_ABORT;

	/* the user chosen filename is moz->moz_unknown_ctrl->filename */
	nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
	NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

	nsCString s ( moz->moz_unknown_ctrl->filename );
	file->InitWithNativePath( s );
	if( !file ) return NS_ERROR_FAILURE;

	*_retval = file;
	NS_ADDREF( *_retval );

	/* add this download to our list */
	EmbedDownload *download = new EmbedDownload( moz, moz->moz_unknown_ctrl->download_ticket, url );
	download->mLauncher = aLauncher;
	aLauncher->SetWebProgressListener( download );

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
	if (NS_FAILED(rv))
		return rv;

	// register this factory with the component registrar.
	nsCOMPtr<nsIComponentRegistrar> registrar;
	rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
	if (NS_FAILED(rv))
		return rv;
	rv = registrar->RegisterFactory( kCID, "nsUnknownContentTypeHandler", NS_IHELPERAPPLAUNCHERDLG_CONTRACTID, factory);
	return NS_OK;
	}

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
 *   Adrian Mardare <amardare@qnx.com>
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

#include "nsILocalFile.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "necko/nsNetUtil.h"
#include "HeaderSniffer.h"
#include "EmbedDownload.h"

const char* const kPersistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

HeaderSniffer::HeaderSniffer( nsIWebBrowserPersist* aPersist, PtMozillaWidget_t *aMoz, nsIURI* aURL, nsIFile* aFile )
: mPersist(aPersist)
, mTmpFile(aFile)
, mURL(aURL)
{
	mMozillaWidget = aMoz;
}

HeaderSniffer::~HeaderSniffer( )
{
}

NS_IMPL_ISUPPORTS1(HeaderSniffer, nsIWebProgressListener)


/* nsIWebProgressListener interface */

NS_IMETHODIMP HeaderSniffer::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress) {
	return NS_OK;
	}

NS_IMETHODIMP HeaderSniffer::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus) {

  if( aStateFlags & nsIWebProgressListener::STATE_START ) {

		nsCOMPtr<nsIWebBrowserPersist> kungFuDeathGrip(mPersist);   // be sure to keep it alive while we save
		                                                            // since it owns us as a listener
		nsCOMPtr<nsIWebProgressListener> kungFuSuicideGrip(this);   // and keep ourselves alive

    nsresult rv;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
    if (!channel) return rv;

		nsCAutoString contentType, suggested_filename;
    channel->GetContentType(contentType);

    // Get the content-disposition if we're an HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
			nsCAutoString contentDisposition;
      nsresult rv = httpChannel->GetResponseHeader(nsCAutoString("content-disposition"), contentDisposition);
			if( NS_SUCCEEDED(rv) && !contentDisposition.IsEmpty() ) {
    		PRInt32 index = contentDisposition.Find("filename=");
    		if( index >= 0 ) {
    		  // Take the substring following the prefix.
    		  index += 9;
    		  contentDisposition.Right(suggested_filename, contentDisposition.Length() - index);
					}
				}
			}

		/* make the client display a file selection dialog */
		PtCallbackInfo_t cbinfo;
		PtWebUnknownWithNameCallback_t cb;

		memset( &cbinfo, 0, sizeof( cbinfo ) );
		cbinfo.reason = Pt_CB_MOZ_UNKNOWN;
		cbinfo.cbdata = &cb;
		cb.action = Pt_WEB_ACTION_OK;
		cb.content_type = (char*)contentType.get();
		nsCAutoString spec; mURL->GetSpec( spec );
		cb.url = (char*)spec.get();
		cb.content_length = strlen( cb.url );
		cb.suggested_filename = (char*)suggested_filename.get();
		PtInvokeCallbackList( mMozillaWidget->web_unknown_cb, (PtWidget_t *)mMozillaWidget, &cbinfo );
		/* this will modal wait for a Pt_ARG_WEB_UNKNOWN_RESP, in mozserver */	
	

		/* cancel the initial save - we used it only to sniff the header */
		mPersist->CancelSave();
		
		/* remove the tmp file */
		PRBool exists;
		mTmpFile->Exists(&exists);
		if(exists)
		    mTmpFile->Remove(PR_FALSE);

		/* we have the result in mMozillaWidget->moz_unknown_ctrl */
		if( mMozillaWidget->moz_unknown_ctrl->response == Pt_WEB_RESPONSE_OK ) {
			/* start the actual save */
			
			/* the user chosen filename is mMozillaWidget->moz_unknown_ctrl->filename */
			nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
			NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

			nsCString s ( mMozillaWidget->moz_unknown_ctrl->filename );
			file->InitWithNativePath( s );
			if( !file ) return NS_ERROR_FAILURE;

			/* add this download to our list */
			nsCAutoString spec;
			mURL->GetSpec( spec );

			nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(kPersistContractID, &rv));
			EmbedDownload *download = new EmbedDownload( mMozillaWidget, mMozillaWidget->moz_unknown_ctrl->download_ticket, spec.get() );
			if( webPersist ) {
				download->mPersist = webPersist;
				webPersist->SetProgressListener( download );
				webPersist->SaveURI( mURL, nsnull, nsnull, nsnull, nsnull, file );
				}
			}
  	}
	return NS_OK;
	}

NS_IMETHODIMP HeaderSniffer::OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsIURI *location) {
	return NS_OK;
	}
NS_IMETHODIMP HeaderSniffer::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsresult aStatus, const PRUnichar* aMessage) {
	return NS_OK;
	}
NS_IMETHODIMP HeaderSniffer::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state) {
	return NS_OK;
	}


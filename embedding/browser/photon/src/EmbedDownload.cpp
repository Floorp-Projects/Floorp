/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "EmbedDownload.h"

EmbedDownload::EmbedDownload( PtMozillaWidget_t *aMoz, int aDownloadTicket, const char * aURL )
{
	mMozillaWidget = aMoz;
	mDownloadTicket = aDownloadTicket;
	mURL = strdup( aURL );
	mDone = PR_FALSE;
	mLauncher = nsnull;
	mPersist = nsnull;

	/* insert d into aMoz->fDownload */
	aMoz->fDownload = ( EmbedDownload ** ) realloc( aMoz->fDownload, ( aMoz->fDownloadCount + 1 ) * sizeof( EmbedDownload * ) );
	if( aMoz->fDownload ) {
		aMoz->fDownload[ aMoz->fDownloadCount ] = this;
		aMoz->fDownloadCount++;
		}
}

EmbedDownload::~EmbedDownload()
{
	int i;

///* ATENTIE */ printf( "EmbedDownload destructor this=%p\n", this );

  /* remove d from the mMoz->fDownload */
  for( i=0; i<mMozillaWidget->fDownloadCount; i++ ) {
    if( mMozillaWidget->fDownload[i] == this ) break;
    }

  if( i<mMozillaWidget->fDownloadCount ) {
    int j;

    for( j=i; j<mMozillaWidget->fDownloadCount-1; j++ )
      mMozillaWidget->fDownload[j] = mMozillaWidget->fDownload[j+1];

    mMozillaWidget->fDownloadCount--;
    if( !mMozillaWidget->fDownloadCount ) {
      free( mMozillaWidget->fDownload );
      mMozillaWidget->fDownload = NULL;
      }

		if( mDone == PR_FALSE ) ReportDownload( Pt_WEB_DOWNLOAD_CANCEL, 0, 0, "" );
    }

///* ATENTIE */ printf( "after remove fDownloadCount=%d\n", mMozillaWidget->fDownloadCount );

	free( mURL );
}

EmbedDownload *FindDownload( PtMozillaWidget_t *moz, int download_ticket )
{
	int i;
  for( i=0; i<moz->fDownloadCount; i++ ) {
    if( moz->fDownload[i]->mDownloadTicket == download_ticket ) return moz->fDownload[i];
    }
	return NULL;
}


void EmbedDownload::ReportDownload( int type, int current, int total, char *message )
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

	if( type == Pt_WEB_DOWNLOAD_DONE ) mDone = PR_TRUE;

///* ATENTIE */ printf( "In EmbedDownload::ReportDownload type=%s\n",
//type==Pt_WEB_DOWNLOAD_CANCEL? "Pt_WEB_DOWNLOAD_CANCEL":
//type==Pt_WEB_DOWNLOAD_DONE? "Pt_WEB_DOWNLOAD_DONE":
//type==Pt_WEB_DOWNLOAD_PROGRESS? "Pt_WEB_DOWNLOAD_PROGRESS":"unknown");

  PtInvokeCallbackList( mMozillaWidget->web_download_cb, (PtWidget_t *)mMozillaWidget, &cbinfo );
  }


/* nsIWebProgressListener interface */
NS_IMPL_ISUPPORTS2(EmbedDownload, nsIWebProgressListener, nsIWebProgressListener2)

NS_IMETHODIMP EmbedDownload::OnProgressChange(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt32 curSelfProgress, PRInt32 maxSelfProgress, PRInt32 curTotalProgress, PRInt32 maxTotalProgress) {

///* ATENTIE */ printf("this=%p OnProgressChange curSelfProgress=%d maxSelfProgress=%d curTotalProgress=%d maxTotalProgress=%d\n",
//this, curSelfProgress, maxSelfProgress, curTotalProgress, maxTotalProgress );

	ReportDownload( Pt_WEB_DOWNLOAD_PROGRESS, curSelfProgress, maxSelfProgress, "" );

	return NS_OK;
	}

NS_IMETHODIMP EmbedDownload::OnProgressChange64(nsIWebProgress *aProgress, nsIRequest *aRequest, PRInt64 curSelfProgress, PRInt64 maxSelfProgress, PRInt64 curTotalProgress, PRInt64 maxTotalProgress) {
  // XXX truncates 64-bit to 32-bit
  return OnProgressChange(aProgress, aRequest,
                          PRInt32(curSelfProgress), PRInt32(maxSelfProgress),
                          PRInt32(curTotalProgress), PRInt32(maxTotalProgress));
  }

NS_IMETHODIMP EmbedDownload::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus) {
	if( aStateFlags & STATE_STOP ) {
		ReportDownload( Pt_WEB_DOWNLOAD_DONE, 0, 0, "" );
		}
	return NS_OK;
	}

NS_IMETHODIMP EmbedDownload::OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsIURI *location) {
	return NS_OK;
	}
NS_IMETHODIMP EmbedDownload::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest, nsresult aStatus, const PRUnichar* aMessage) {
	return NS_OK;
	}
NS_IMETHODIMP EmbedDownload::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state) {
	return NS_OK;
	}
NS_IMETHODIMP EmbedDownload::OnRefreshAttempted(nsIWebProgress *aWebProgress, nsIURI *aUri, PRInt32 aDelay, PRBool aSameUri, PRBool *allowRefresh)
{
    *allowRefresh = PR_TRUE;
    return NS_OK;
}


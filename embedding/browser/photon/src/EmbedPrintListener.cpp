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

#include <strings.h>
#include <nsXPIDLString.h>

#include "nsIURI.h"

#include "EmbedPrintListener.h"
#include "EmbedPrivate.h"

#include "PtMozilla.h"

EmbedPrintListener::EmbedPrintListener(void)
{
  mOwner = nsnull;
}

EmbedPrintListener::~EmbedPrintListener()
{
}

NS_IMPL_ISUPPORTS1(EmbedPrintListener, nsIWebProgressListener)

nsresult
EmbedPrintListener::Init(EmbedPrivate *aOwner)
{
	mOwner = aOwner;
	return NS_OK;
}

void 
EmbedPrintListener::InvokePrintCallback(int status, unsigned int cur, unsigned int max)
{
	PtMozillaWidget_t   *moz = (PtMozillaWidget_t *) mOwner->mOwningWidget;
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

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP EmbedPrintListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{
  if (aStatus == nsIWebProgressListener::STATE_START) {
	  InvokePrintCallback(Pt_MOZ_PRINT_START, 0, 0);

  } else if (aStatus == nsIWebProgressListener::STATE_STOP) {
	  InvokePrintCallback(Pt_MOZ_PRINT_COMPLETE, 0, 0);
  }
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP EmbedPrintListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
	InvokePrintCallback(Pt_MOZ_PRINT_PROGRESS, aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP EmbedPrintListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP EmbedPrintListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP EmbedPrintListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

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
  NS_INIT_REFCNT();
  mOwner = nsnull;
}

EmbedPrintListener::~EmbedPrintListener()
{
}

NS_IMPL_ISUPPORTS1(EmbedPrintListener, nsIPrintListener)

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

/* void OnStartPrinting (); */
NS_IMETHODIMP 
EmbedPrintListener::OnStartPrinting()
{
	InvokePrintCallback(Pt_MOZ_PRINT_START, 0, 0);
    return NS_OK;
}

/* void OnProgressPrinting (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP 
EmbedPrintListener::OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax)
{
	InvokePrintCallback(Pt_MOZ_PRINT_PROGRESS, aProgress, aProgressMax);
    return NS_OK;
}

/* void OnEndPrinting (in PRUint32 aStatus); */
NS_IMETHODIMP 
EmbedPrintListener::OnEndPrinting(PRUint32 aStatus)
{
	InvokePrintCallback(Pt_MOZ_PRINT_COMPLETE, 0, 0);
    return NS_OK;
}

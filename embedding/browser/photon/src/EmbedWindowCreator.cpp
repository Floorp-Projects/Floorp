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

#include "EmbedWindowCreator.h"
#include "EmbedPrivate.h"
#include "EmbedWindow.h"

// in order to create orphaned windows
#include "PtMozilla.h"

EmbedWindowCreator::EmbedWindowCreator(void)
{
	NS_INIT_ISUPPORTS();
}

EmbedWindowCreator::~EmbedWindowCreator()
{
}

NS_IMPL_ISUPPORTS1(EmbedWindowCreator, nsIWindowCreator)

NS_IMETHODIMP
EmbedWindowCreator::CreateChromeWindow(nsIWebBrowserChrome *aParent,
				       PRUint32 aChromeFlags,
				       nsIWebBrowserChrome **_retval)
{
	NS_ENSURE_ARG_POINTER(_retval);

	EmbedPrivate 			*embedPrivate = EmbedPrivate::FindPrivateForBrowser(aParent);
	PtMozillaWidget_t       *nmoz, *moz;
	PtCallbackList_t        *cb;
	PtCallbackInfo_t        cbinfo;
	PtMozillaNewWindowCb_t  nwin;

	if (!embedPrivate)
		return NS_ERROR_FAILURE;

	moz = (PtMozillaWidget_t *)embedPrivate->mOwningWidget;
	if (!moz || !moz->new_window_cb)
		return NS_ERROR_FAILURE;

	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.cbdata = &nwin;
	cbinfo.reason = Pt_CB_MOZ_NEW_WINDOW;
	cb = moz->new_window_cb;
	nwin.window_flags = aChromeFlags;

	/* the size will be the same as the main window, and it might be resized later by Pt_CB_WEB_NEW_AREA */
	nwin.window_size.w = nwin.window_size.h = 0;

	PtSetParentWidget(NULL);
	if (PtInvokeCallbackList(cb, (PtWidget_t *) moz, &cbinfo) == Pt_CONTINUE)
	{
	nmoz = (PtMozillaWidget_t *) nwin.widget;
		EmbedPrivate *newEmbedPrivate = nmoz->EmbedRef;

		if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
			newEmbedPrivate->mIsChrome = PR_TRUE;

		*_retval = NS_STATIC_CAST(nsIWebBrowserChrome *, (newEmbedPrivate->mWindow));

		if (*_retval) 
		{
			NS_ADDREF(*_retval);
			return NS_OK;
		}
	}

	return NS_ERROR_FAILURE;
}

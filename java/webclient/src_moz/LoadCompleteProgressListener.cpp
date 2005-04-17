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
 */

#include "LoadCompleteProgressListener.h"

#include <nsXPIDLString.h>
#include <nsIChannel.h>
#include <nsIHttpChannel.h>
#include <nsIUploadChannel.h>
#include <nsIInputStream.h>
#include <nsISeekableStream.h>
#include <nsIHttpHeaderVisitor.h>

#include "nsIURI.h"
#include "nsCRT.h"

#include "nsString.h"

#include "NativeBrowserControl.h"

#include "HttpHeaderVisitorImpl.h"

#include "ns_globals.h" // for prLogModuleInfo

LoadCompleteProgressListener::LoadCompleteProgressListener(void)
{
  mOwner = nsnull;
  mLoadComplete = PR_FALSE;
}

LoadCompleteProgressListener::~LoadCompleteProgressListener()
{ 
    mOwner = nsnull;
    mLoadComplete = PR_FALSE;
}

NS_IMPL_ISUPPORTS2(LoadCompleteProgressListener,
		   nsIWebProgressListener,
		   nsISupportsWeakReference)

nsresult
LoadCompleteProgressListener::Init(NativeBrowserControl *aOwner)
{
  mOwner = aOwner;

  return NS_OK;
}


NS_IMETHODIMP
LoadCompleteProgressListener::OnStateChange(nsIWebProgress *aWebProgress,
			     nsIRequest     *aRequest,
			     PRUint32        aStateFlags,
			     nsresult        aStatus)
{

    if ((aStateFlags & STATE_IS_NETWORK) && 
	(aStateFlags & STATE_START)) {
	mLoadComplete = PR_FALSE;
    }

    if ((aStateFlags & STATE_IS_NETWORK) && 
	(aStateFlags & STATE_STOP)) {
	mLoadComplete = PR_TRUE;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
LoadCompleteProgressListener::OnProgressChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRInt32         aCurSelfProgress,
				PRInt32         aMaxSelfProgress,
				PRInt32         aCurTotalProgress,
				PRInt32         aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP
LoadCompleteProgressListener::OnLocationChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				nsIURI         *aLocation)
{
    return NS_OK;
}



NS_IMETHODIMP
LoadCompleteProgressListener::OnStatusChange(nsIWebProgress  *aWebProgress,
			      nsIRequest      *aRequest,
			      nsresult         aStatus,
			      const PRUnichar *aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP
LoadCompleteProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress,
				nsIRequest     *aRequest,
				PRUint32         aState)
{
    return NS_OK;
}

nsresult
LoadCompleteProgressListener::IsLoadComplete(PRBool *_retval)
{
    if (!_retval) {
	return NS_ERROR_NULL_POINTER;
    }
    
    *_retval = mLoadComplete;
    return NS_OK;
}


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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsCopyMessageStreamListener.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMailboxUrl.h"
#include "nsIMsgHdr.h"
#include "nsIRDFService.h"
#include "nsIRDFNode.h"
#include "nsRDFCID.h"
#include "nsIMsgImapMailFolder.h"
#include "nsXPIDLString.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_THREADSAFE_ADDREF(nsCopyMessageStreamListener)
NS_IMPL_THREADSAFE_RELEASE(nsCopyMessageStreamListener)

NS_INTERFACE_MAP_BEGIN(nsCopyMessageStreamListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsICopyMessageStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

static nsresult GetMessage(nsIURI *aURL, nsIMsgDBHdr **message)
{
	nsCOMPtr<nsIMsgMessageUrl> uriURL;
	nsXPIDLCString uri;
	nsresult rv;

	if(!message)
		return NS_ERROR_NULL_POINTER;

	//Need to get message we are about to copy
	uriURL = do_QueryInterface(aURL, &rv);
	if(NS_FAILED(rv))
		return rv;

	rv = uriURL->GetUri(getter_Copies(uri));
	if(NS_FAILED(rv))
		return rv;

  nsCOMPtr <nsIMsgMessageService> msgMessageService;
  rv = GetMessageServiceFromURI(uri, getter_AddRefs(msgMessageService));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!msgMessageService) return NS_ERROR_FAILURE;

  return msgMessageService->MessageURIToMsgHdr(uri, message);
}

nsCopyMessageStreamListener::nsCopyMessageStreamListener()
{
  /* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();
}

nsCopyMessageStreamListener::~nsCopyMessageStreamListener()
{
	//All member variables are nsCOMPtr's.
}

NS_IMETHODIMP nsCopyMessageStreamListener::Init(nsIMsgFolder *srcFolder, nsICopyMessageListener *destination, nsISupports *listenerData)
{
	mSrcFolder = dont_QueryInterface(srcFolder);
	mDestination = dont_QueryInterface(destination);
	mListenerData = dont_QueryInterface(listenerData);
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::StartMessage()
{
	if (mDestination)
		mDestination->StartMessage();

	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::EndMessage(nsMsgKey key)
{
	if (mDestination)
		mDestination->EndMessage(key);

	return NS_OK;
}


NS_IMETHODIMP nsCopyMessageStreamListener::OnDataAvailable(nsIRequest * /* request */, nsISupports *ctxt, nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	nsresult rv;
	rv = mDestination->CopyData(aIStream, aLength);
	return rv;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStartRequest(nsIRequest * request, nsISupports *ctxt)
{
	nsCOMPtr<nsIMsgDBHdr> message;
	nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> uri = do_QueryInterface(ctxt, &rv);

  NS_ASSERTION(NS_SUCCEEDED(rv), "ahah...someone didn't pass in the expected context!!!");
	
	if (NS_SUCCEEDED(rv))
		rv = GetMessage(uri, getter_AddRefs(message));
	if(NS_SUCCEEDED(rv))
		rv = mDestination->BeginCopy(message);

  NS_ENSURE_SUCCESS(rv, rv);
	return rv;
}

NS_IMETHODIMP nsCopyMessageStreamListener::EndCopy(nsISupports *url, nsresult aStatus)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> uri = do_QueryInterface(url, &rv);

	if (NS_FAILED(rv)) return rv;
	PRBool copySucceeded = (aStatus == NS_BINDING_SUCCEEDED);
	rv = mDestination->EndCopy(copySucceeded);
	//If this is a move and we finished the copy, delete the old message.
	if(NS_SUCCEEDED(rv) && copySucceeded)
	{
		PRBool moveMessage = PR_FALSE;

		nsCOMPtr<nsIMsgMailNewsUrl> mailURL(do_QueryInterface(uri));
		if(mailURL)
			rv = mailURL->IsUrlType(nsIMsgMailNewsUrl::eMove, &moveMessage);

		if(NS_FAILED(rv))
			moveMessage = PR_FALSE;

		// OK, this is wrong if we're moving to an imap folder, for example. This really says that
		// we were able to pull the message from the source, NOT that we were able to
		// put it in the destination!
		if(moveMessage)
		{
			// don't do this if we're moving to an imap folder - that's handled elsewhere.
			nsCOMPtr <nsIMsgImapMailFolder> destImap = do_QueryInterface(mDestination);
			if (!destImap)
			{
        // if the destination is a local folder, it will handle the delete from the source in EndMove
//				rv = DeleteMessage(uri, mSrcFolder);
//				if(NS_SUCCEEDED(rv))
					rv = mDestination->EndMove();
			}
		}
	}
	//Even if the above actions failed we probably still want to return NS_OK.  There should probably
	//be some error dialog if either the copy or delete failed.
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStopRequest(nsIRequest* request, nsISupports *ctxt, nsresult aStatus)
{
  return EndCopy(ctxt, aStatus);
}


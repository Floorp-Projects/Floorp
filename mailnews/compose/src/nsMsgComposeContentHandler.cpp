/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Jean-Francois Ducarroz <ducarroz@netscape.com>
 */

#include "nsMsgComposeContentHandler.h"
#include "nsMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsIChannel.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kMsgComposeServiceCID, NS_MSGCOMPOSESERVICE_CID);

nsMsgComposeContentHandler::nsMsgComposeContentHandler()
{
	NS_INIT_REFCNT();
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS1(nsMsgComposeContentHandler, nsIContentHandler);

nsMsgComposeContentHandler::~nsMsgComposeContentHandler()
{
}

NS_IMETHODIMP nsMsgComposeContentHandler::HandleContent(const char * aContentType, const char * aCommand,
                                                const char * aWindowTarget, nsISupports * aWindowContext, nsIRequest *request)
{
  nsresult rv = NS_OK;
  if (!request)
    return NS_ERROR_NULL_POINTER;

  // First of all, get the content type and make sure it is a content type we know how to handle!
  if (nsCRT::strcasecmp(aContentType, "x-application-mailto") == 0) {
    nsCOMPtr<nsIURI> aUri;
    nsCOMPtr<nsIChannel> aChannel;
    request->GetParent(getter_AddRefs(aChannel));
    if(!aChannel) return NS_ERROR_FAILURE;

    rv = aChannel->GetURI(getter_AddRefs(aUri));
    if (aUri)
    {
      NS_WITH_SERVICE(nsIMsgComposeService, composeService, kMsgComposeServiceCID, &rv)
      if (NS_SUCCEEDED(rv))
        rv = composeService->OpenComposeWindowWithURI(nsnull, aUri);
    }
  }

  return rv;
}

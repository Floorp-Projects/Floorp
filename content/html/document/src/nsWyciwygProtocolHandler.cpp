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
 * Contributor(s): Radha Kulkarni(radha@netscape.com)
 */

#include "nsWyciwygChannel.h"
#include "nsWyciwygProtocolHandler.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsWyciwygProtocolHandler::nsWyciwygProtocolHandler() 
{
  NS_INIT_ISUPPORTS();

#if defined(PR_LOGGING)
  gWyciwygLog = PR_NewLogModule("nsWyciwygChannel");
#endif
}

nsWyciwygProtocolHandler::~nsWyciwygProtocolHandler() 
{
}

NS_IMPL_ISUPPORTS1(nsWyciwygProtocolHandler, nsIProtocolHandler);

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetScheme(nsACString &result)
{
  result = "wyciwyg";
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetDefaultPort(PRInt32 *result) 
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsWyciwygProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
  // don't override anything.  
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewURI(const nsACString &aSpec,
                                 const char *aCharset, // ignored
                                 nsIURI *aBaseURI,
                                 nsIURI **result) 
{
  nsresult rv;

  nsCOMPtr<nsIURI> url = do_CreateInstance(kSimpleURICID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = url->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = url;
  NS_ADDREF(*result);

  return rv;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
  nsresult rv;
    
  nsWyciwygChannel* channel = new nsWyciwygChannel();
  if (!channel)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(channel);
  rv = channel->Init(url);
  if (NS_FAILED(rv)) {
    NS_RELEASE(channel);
    return rv;
  }

  *result = channel;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetProtocolFlags(PRUint32 *result) 
{
  *result = URI_NORELATIVE | URI_NOAUTH;
  return NS_OK;
}

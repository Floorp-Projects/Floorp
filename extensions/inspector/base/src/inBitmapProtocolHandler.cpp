/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Joe Hewitt <hewitt@netscape.com>
 */

#include "inBitmapChannel.h"
#include "inBitmapURI.h"
#include "inBitmapProtocolHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"

////////////////////////////////////////////////////////////////////////////////

inBitmapProtocolHandler::inBitmapProtocolHandler() 
{
  NS_INIT_REFCNT();
}

inBitmapProtocolHandler::~inBitmapProtocolHandler() 
{}

NS_IMPL_ISUPPORTS2(inBitmapProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference)

    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler

NS_IMETHODIMP inBitmapProtocolHandler::GetScheme(char* *result) 
{
  *result = nsCRT::strdup("moz-bitmap");
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP inBitmapProtocolHandler::GetDefaultPort(PRInt32 *result) 
{
  *result = 0;
  return NS_OK;
}

NS_IMETHODIMP inBitmapProtocolHandler::GetProtocolFlags(PRUint32 *result) 
{
  *result = URI_NORELATIVE;
  return NS_OK;
}

NS_IMETHODIMP inBitmapProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP inBitmapProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **result) 
{
  // no concept of a relative bitmap url
  NS_ASSERTION(!aBaseURI, "base url passed into bitmap protocol handler");
  
  nsCOMPtr<nsIURI> uri;
  NS_NEWXPCOM(uri, inBitmapURI);
  if (!uri) return NS_ERROR_FAILURE;

  uri->SetSpec(aSpec);

  *result = uri;
  NS_IF_ADDREF(*result);
  return NS_OK;
}

NS_IMETHODIMP inBitmapProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
  nsCOMPtr<nsIChannel> channel;
  NS_NEWXPCOM(channel, inBitmapChannel);

  if (channel)
    NS_STATIC_CAST(inBitmapChannel*,NS_STATIC_CAST(nsIChannel*, channel))->Init(url);

  *result = channel;
  NS_IF_ADDREF(*result);

  return NS_OK;
}

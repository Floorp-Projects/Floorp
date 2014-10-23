/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconChannel.h"
#include "nsIconURI.h"
#include "nsIconProtocolHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsNetCID.h"

////////////////////////////////////////////////////////////////////////////////

nsIconProtocolHandler::nsIconProtocolHandler() 
{
}

nsIconProtocolHandler::~nsIconProtocolHandler() 
{}

NS_IMPL_ISUPPORTS(nsIconProtocolHandler, nsIProtocolHandler, nsISupportsWeakReference)

    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP nsIconProtocolHandler::GetScheme(nsACString &result) 
{
  result = "moz-icon";
  return NS_OK;
}

NS_IMETHODIMP nsIconProtocolHandler::GetDefaultPort(int32_t *result) 
{
  *result = 0;
  return NS_OK;
}

NS_IMETHODIMP nsIconProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}

NS_IMETHODIMP nsIconProtocolHandler::GetProtocolFlags(uint32_t *result)
{
  *result = URI_NORELATIVE | URI_NOAUTH | URI_IS_UI_RESOURCE |
            URI_IS_LOCAL_RESOURCE;
  return NS_OK;
}

NS_IMETHODIMP nsIconProtocolHandler::NewURI(const nsACString &aSpec,
                                            const char *aOriginCharset, // ignored
                                            nsIURI *aBaseURI,
                                            nsIURI **result) 
{
  
  nsCOMPtr<nsIURI> uri = new nsMozIconURI();
  if (!uri) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = uri->SetSpec(aSpec);
  if (NS_FAILED(rv)) return rv;

  NS_ADDREF(*result = uri);
  return NS_OK;
}

NS_IMETHODIMP
nsIconProtocolHandler::NewChannel2(nsIURI* url,
                                   nsILoadInfo* aLoadInfo,
                                   nsIChannel** result)
{
  NS_ENSURE_ARG_POINTER(url);
  nsIconChannel* channel = new nsIconChannel;
  if (!channel)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(channel);

  nsresult rv = channel->Init(url);
  if (NS_FAILED(rv)) {
    NS_RELEASE(channel);
    return rv;
  }

  *result = channel;
  return NS_OK;
}

NS_IMETHODIMP nsIconProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
  return NewChannel2(url, nullptr, result);
}

////////////////////////////////////////////////////////////////////////////////

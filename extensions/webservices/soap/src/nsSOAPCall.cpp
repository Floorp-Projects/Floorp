/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSOAPCall.h"
#include "nsSOAPResponse.h"
#include "nsSOAPUtils.h"
#include "nsISOAPTransport.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsSOAPException.h"

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsSOAPCall::nsSOAPCall() : mVerifySourceHeader(PR_FALSE)
{
}

nsSOAPCall::~nsSOAPCall()
{
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPCall, nsISOAPMessage, nsISOAPCall)
    NS_IMPL_ADDREF_INHERITED(nsSOAPCall, nsSOAPMessage)
    NS_IMPL_RELEASE_INHERITED(nsSOAPCall, nsSOAPMessage)
    NS_INTERFACE_MAP_BEGIN(nsSOAPCall)
    NS_INTERFACE_MAP_ENTRY(nsISOAPCall)
    NS_IMPL_QUERY_CLASSINFO(nsSOAPCall)
    NS_INTERFACE_MAP_END_INHERITING(nsSOAPMessage)
/* attribute boolean verifySourceHeader; */
NS_IMETHODIMP nsSOAPCall::GetVerifySourceHeader(PRBool * aVerifySourceHeader)
{
  NS_ENSURE_ARG_POINTER(aVerifySourceHeader);
  *aVerifySourceHeader = mVerifySourceHeader;
  return NS_OK;
}

NS_IMETHODIMP nsSOAPCall::SetVerifySourceHeader(PRBool aVerifySourceHeader)
{
  mVerifySourceHeader = aVerifySourceHeader;
  return NS_OK;
}

/* attribute DOMString transportURI; */
NS_IMETHODIMP nsSOAPCall::GetTransportURI(nsAString & aTransportURI)
{
  aTransportURI.Assign(mTransportURI);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPCall::SetTransportURI(const nsAString & aTransportURI)
{
  mTransportURI.Assign(aTransportURI);
  return NS_OK;
}

nsresult nsSOAPCall::GetTransport(nsISOAPTransport ** aTransport)
{
  NS_ENSURE_ARG_POINTER(aTransport);
  *aTransport = nsnull;

  nsresult rv;
  nsCOMPtr<nsIURI> uri;

  rv = NS_NewURI(getter_AddRefs(uri), mTransportURI);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString protocol;
  uri->GetScheme(protocol);

  nsCAutoString transportContractid;
  transportContractid.Assign(NS_SOAPTRANSPORT_CONTRACTID_PREFIX);
  transportContractid.Append(protocol);

  return CallGetService(transportContractid.get(), aTransport);
}

/* nsISOAPResponse invoke (); */
NS_IMETHODIMP nsSOAPCall::Invoke(nsISOAPResponse ** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv;
  nsCOMPtr<nsISOAPTransport> transport;

  if (mTransportURI.IsEmpty()) {
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_TRANSPORT_URI", "No transport URI was specified.");
  }

  rv = GetTransport(getter_AddRefs(transport));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISOAPResponse>
      response(do_CreateInstance(NS_SOAPRESPONSE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = response->SetEncoding(mEncoding);
  if (NS_FAILED(rv))
    return rv;

  rv = transport->SyncCall(this, response);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMDocument> document;
  rv = response->GetMessage(getter_AddRefs(document));        //  No XML response.
  if (NS_FAILED(rv))
    return rv;
  if (!document) {
    *_retval = nsnull;
    return NS_OK;
  }

  return response->QueryInterface(NS_GET_IID(nsISOAPResponse),
                                  (void **) _retval);
}

/* void asyncInvoke (in nsISOAPResponseListener listener); */
NS_IMETHODIMP
    nsSOAPCall::AsyncInvoke(nsISOAPResponseListener * listener,
                            nsISOAPCallCompletion ** aCompletion)
{
  NS_ENSURE_ARG_POINTER(aCompletion);
  nsresult rv;
  nsCOMPtr<nsISOAPTransport> transport;

  if (mTransportURI.IsEmpty()) {
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_TRANSPORT_URI", "No transport URI was specified.");
  }

  rv = GetTransport(getter_AddRefs(transport));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISOAPResponse>
      response(do_CreateInstance(NS_SOAPRESPONSE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  rv = response->SetEncoding(mEncoding);
  if (NS_FAILED(rv))
    return rv;

  rv = transport->AsyncCall(this, listener, response, aCompletion);
  return rv;
}

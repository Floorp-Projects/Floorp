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
 */

#include "nsHTTPSOAPTransport.h"
#include "nsIComponentManager.h"
#include "nsIDOMDocument.h"
#include "nsIVariant.h"
#include "nsString.h"
#include "nsSOAPUtils.h"
#include "nsSOAPCall.h"
#include "nsSOAPResponse.h"
#include "nsIDOMEventTarget.h"

nsHTTPSOAPTransport::nsHTTPSOAPTransport()
{
  NS_INIT_ISUPPORTS();
}

nsHTTPSOAPTransport::~nsHTTPSOAPTransport()
{
}

NS_IMPL_ISUPPORTS1_CI(nsHTTPSOAPTransport, nsISOAPTransport)

/* void syncCall (in nsISOAPCall aCall, in nsISOAPResponse aResponse); */
NS_IMETHODIMP nsHTTPSOAPTransport::SyncCall(nsISOAPCall *aCall, nsISOAPResponse *aResponse)
{
  NS_ENSURE_ARG(aCall);

  nsresult rv;
  nsCOMPtr<nsIXMLHttpRequest> request;

  request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString action;
  rv = aCall->GetActionURI(action);
  if (NS_FAILED(rv)) return rv;
  if (!AStringIsNull(action)) {
    rv = request->SetRequestHeader("SOAPAction", NS_ConvertUCS2toUTF8(action).get());
    if (NS_FAILED(rv)) return rv;
  }

  nsAutoString uri;
  rv = aCall->GetTransportURI(uri);
  if (NS_FAILED(rv)) return rv;
  if (!AStringIsNull(uri)) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMDocument> messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv)) return rv;
  if (!messageDocument) return NS_ERROR_NOT_INITIALIZED;

  rv = request->OpenRequest("POST", NS_ConvertUCS2toUTF8(uri).get(), PR_FALSE, nsnull, nsnull);
  if (NS_FAILED(rv)) return rv;
			    
  nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = variant->SetAsInterface(NS_GET_IID(nsIDOMDocument), messageDocument);
  if (NS_FAILED(rv)) return rv;

  rv = request->Send(variant);
  if (NS_FAILED(rv)) return rv;

  request->GetStatus(&rv);
  if (NS_FAILED(rv)) return rv;

  if (aResponse) {
    nsCOMPtr<nsIDOMDocument> response;
    rv = request->GetResponseXML(getter_AddRefs(response));
    if (NS_FAILED(rv)) return rv;
    rv = aResponse->SetMessage(response);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

class nsHTTPSOAPTransportCompletion : public nsIDOMEventListener
{
public:
  nsHTTPSOAPTransportCompletion();
  virtual ~nsHTTPSOAPTransportCompletion();

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

protected:
  nsCOMPtr<nsISOAPCall> mCall;
  nsCOMPtr<nsISOAPResponse> mResponse;
  nsCOMPtr<nsIXMLHttpRequest> mRequest;
  nsCOMPtr<nsISOAPResponseListener> mListener;
};

NS_IMPL_ISUPPORTS1(nsHTTPSOAPTransportCompletion, nsIDOMEventListener)

nsHTTPSOAPTransportCompletion::nsHTTPSOAPTransportCompletion()
{
  NS_INIT_ISUPPORTS();
}
nsHTTPSOAPTransportCompletion::~nsHTTPSOAPTransportCompletion()
{
}
NS_IMETHODIMP
nsHTTPSOAPTransportCompletion::HandleEvent(nsIDOMEvent* aEvent)
{
  nsresult rv;
  mRequest->GetStatus(&rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMDocument> document;
    rv = mRequest->GetResponseXML(getter_AddRefs(document));
    if (NS_SUCCEEDED(rv)) {
      rv = mResponse->SetMessage(document);
    }
  }
  PRBool c;  //  In other transports, this may signal to stop returning if multiple returns
  mListener->HandleResponse(mResponse, mCall, (PRInt32)rv, PR_TRUE, &c);
  return NS_OK;
}

/* void asyncCall (in nsISOAPCall aCall, in nsISOAPResponseListener aListener, in nsISOAPResponse aResponse); */
NS_IMETHODIMP nsHTTPSOAPTransport::AsyncCall(nsISOAPCall *aCall, nsISOAPResponseListener *aListener, nsISOAPResponse *aResponse)
{
  NS_ENSURE_ARG(aCall);

  nsresult rv;
  nsCOMPtr<nsIXMLHttpRequest> request;

  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(request, &rv);
  if (NS_FAILED(rv)) return rv;

  request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString action;
  rv = aCall->GetActionURI(action);
  if (NS_FAILED(rv)) return rv;
  if (!AStringIsNull(action)) {
    rv = request->SetRequestHeader("SOAPAction", NS_ConvertUCS2toUTF8(action).get());
    if (NS_FAILED(rv)) return rv;
  }
  nsCOMPtr<nsIDOMEventListener> listener = new nsHTTPSOAPTransportCompletion();
  if (!listener) return NS_ERROR_OUT_OF_MEMORY;

  nsAutoString uri;
  rv = aCall->GetTransportURI(uri);
  if (NS_FAILED(rv)) return rv;
  if (!AStringIsNull(uri)) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMDocument> messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv)) return rv;
  if (!messageDocument) return NS_ERROR_NOT_INITIALIZED;

  rv = request->OpenRequest("POST", NS_ConvertUCS2toUTF8(uri).get(), PR_TRUE, nsnull, nsnull);
  if (NS_FAILED(rv)) return rv;
			    
  eventTarget->AddEventListener(NS_LITERAL_STRING("load"), listener, PR_FALSE);
  eventTarget->AddEventListener(NS_LITERAL_STRING("error"), listener, PR_FALSE);

  nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = variant->SetAsInterface(NS_GET_IID(nsIDOMDocument), messageDocument);
  if (NS_FAILED(rv)) return rv;

  rv = request->Send(variant);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

/* void addListener (in nsISOAPTransportListener aListener, in boolean aCapture); */
NS_IMETHODIMP nsHTTPSOAPTransport::AddListener(nsISOAPTransportListener *aListener, PRBool aCapture)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeListener (in nsISOAPTransportListener aListener, in boolean aCapture); */
NS_IMETHODIMP nsHTTPSOAPTransport::RemoveListener(nsISOAPTransportListener *aListener, PRBool aCapture)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


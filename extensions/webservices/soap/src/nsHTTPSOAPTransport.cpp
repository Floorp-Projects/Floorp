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
#include "nsIDOMEventTarget.h"

#define LOADSTR NS_LITERAL_STRING("load")
#define ERRORSTR NS_LITERAL_STRING("error")

nsHTTPSOAPTransport::nsHTTPSOAPTransport()
{
  NS_INIT_ISUPPORTS();
  mStatus = 0;
}

nsHTTPSOAPTransport::~nsHTTPSOAPTransport()
{
}

NS_IMPL_ISUPPORTS2(nsHTTPSOAPTransport, nsISOAPTransport, nsIDOMEventListener)

/* boolean canDoSync (); */
NS_IMETHODIMP 
nsHTTPSOAPTransport::CanDoSync(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_TRUE;
  return NS_OK;
}

/* nsIDOMDocument syncCall (in string url, in string action, in
   nsIDOMDocument messageDocument); */
NS_IMETHODIMP 
nsHTTPSOAPTransport::SyncCall(const char *url, 
			      const char *action, 
			      nsIDOMDocument *messageDocument, 
			      nsIDOMDocument **_retval)
{
  NS_ENSURE_ARG(url);
  NS_ENSURE_ARG(messageDocument);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;
  nsCOMPtr<nsIXMLHttpRequest> request;

  request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  if (action) {
    request->SetRequestHeader("SOAPAction", action);
  }

  rv = request->OpenRequest("POST", url, PR_FALSE, nsnull, nsnull);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
			    
  rv = request->Send(messageDocument);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  request->GetStatus(&mStatus);

  rv = request->GetResponseXML(_retval);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  return NS_OK;
}

/* void asyncCall (in string url, in string action, in nsIDOMDocument
   messageDocument, in nsISOAPTransportListener listener); */
NS_IMETHODIMP 
nsHTTPSOAPTransport::AsyncCall(const char *url, 
                               const char *action, 
                               nsIDOMDocument *messageDocument, 
                               nsISOAPTransportListener *listener)
{
  NS_ENSURE_ARG(url);
  NS_ENSURE_ARG(messageDocument);

  nsresult rv;

  mRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  if (action) {
    mRequest->SetRequestHeader("SOAPAction", action);
  }

  rv = mRequest->OpenRequest("POST", url, PR_TRUE, nsnull, nsnull);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  mListener = listener;

  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(mRequest, &rv));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  eventTarget->AddEventListener(LOADSTR, 
			     NS_STATIC_CAST(nsIDOMEventListener*, this), PR_FALSE);
  eventTarget->AddEventListener(ERRORSTR, 
			     NS_STATIC_CAST(nsIDOMEventListener*, this), PR_FALSE);
  
  rv = mRequest->Send(messageDocument);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsHTTPSOAPTransport::GetStatus(PRUint32 *aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);

  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsHTTPSOAPTransport::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMDocument> document;
  PRUint32 status;

  mRequest->GetResponseXML(getter_AddRefs(document));
  mRequest->GetStatus(&status);

  if (mListener) {
    // XXX If this is an error, we need to pass in NS_ERROR_FAILURE
    mListener->HandleResponse(document, status, NS_OK);
    mListener = 0;
  }
  
  mRequest = 0;

  return NS_OK;
}

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
 * Copyright (C) 2001 Netscape Communications Corporation. All
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
#include "nsISOAPCallCompletion.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMSerializer.h"

nsHTTPSOAPTransport::nsHTTPSOAPTransport()
{
  NS_INIT_ISUPPORTS();
}

nsHTTPSOAPTransport::~nsHTTPSOAPTransport()
{
}

NS_IMPL_ISUPPORTS1_CI(nsHTTPSOAPTransport, nsISOAPTransport)
#ifdef DEBUG
#define DEBUG_DUMP_DOCUMENT(message,doc) \
  { \
	  nsresult rcc;\
	  nsXPIDLString serial;\
	  nsCOMPtr<nsIDOMSerializer> serializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rcc));\
	  if (NS_FAILED(rcc)) return rcc;  \
	  rcc = serializer->SerializeToString(doc, getter_Copies(serial));\
	  if (NS_FAILED(rcc)) return rcc;\
	  nsAutoString result(serial);\
	  printf(message ":\n%s\n", NS_ConvertUCS2toUTF8(result).get());\
  }
//  Availble from the debugger...
nsresult DebugPrintDOM(nsIDOMNode * node)
{
  DEBUG_DUMP_DOCUMENT("DOM", node) return NS_OK;
}
#else
#define DEBUG_DUMP_DOCUMENT(message,doc)
#endif
/* void syncCall (in nsISOAPCall aCall, in nsISOAPResponse aResponse); */
NS_IMETHODIMP nsHTTPSOAPTransport::SyncCall(nsISOAPCall * aCall,
					    nsISOAPResponse * aResponse)
{
  NS_ENSURE_ARG(aCall);

  nsresult rv;
  nsCOMPtr < nsIXMLHttpRequest > request;

  nsCOMPtr < nsIDOMDocument > messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv))
    return rv;
  if (!messageDocument)
    return NS_ERROR_NOT_INITIALIZED;

  DEBUG_DUMP_DOCUMENT("Synchronous Request", messageDocument)
      request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString uri;
  rv = aCall->GetTransportURI(uri);
  if (NS_FAILED(rv))
    return rv;
  if (AStringIsNull(uri))
    return NS_ERROR_NOT_INITIALIZED;

  rv = request->OpenRequest("POST", NS_ConvertUCS2toUTF8(uri).get(),
			    PR_FALSE, nsnull, nsnull);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString action;
  rv = aCall->GetActionURI(action);
  if (NS_FAILED(rv))
    return rv;
  if (!AStringIsNull(action)) {
    rv = request->SetRequestHeader("SOAPAction",
				   NS_ConvertUCS2toUTF8(action).get());
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr < nsIWritableVariant > variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = variant->SetAsInterface(NS_GET_IID(nsIDOMDocument),
			       messageDocument);
  if (NS_FAILED(rv))
    return rv;


  rv = request->Send(variant);
  if (NS_FAILED(rv))
    return rv;

#if 0
  PRUint32 status;
  rv = request->GetStatus(&status);
  if (NS_SUCCEEDED(rv) && (status < 200 || status >= 300))
    rv = NS_ERROR_FAILURE;
  if (NS_FAILED(rv))
    return rv;
#endif

  if (aResponse) {
    nsCOMPtr < nsIDOMDocument > response;
    rv = request->GetResponseXML(getter_AddRefs(response));
    if (NS_FAILED(rv))
      return rv;
    if (response) {
    DEBUG_DUMP_DOCUMENT("Asynchronous Response", response)}
    rv = aResponse->SetMessage(response);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS2_CI(nsHTTPSOAPTransportCompletion, nsIDOMEventListener,
		      nsISOAPCallCompletion)
    nsHTTPSOAPTransportCompletion::nsHTTPSOAPTransportCompletion()
{
  NS_INIT_ISUPPORTS();
}

nsHTTPSOAPTransportCompletion::nsHTTPSOAPTransportCompletion(nsISOAPCall * call, nsISOAPResponse * response, nsIXMLHttpRequest * request, nsISOAPResponseListener * listener):
mCall(call), mResponse(response), mRequest(request), mListener(listener)
{
  NS_INIT_ISUPPORTS();
}

nsHTTPSOAPTransportCompletion::~nsHTTPSOAPTransportCompletion()
{
}

/* readonly attribute nsISOAPCall call; */
NS_IMETHODIMP nsHTTPSOAPTransportCompletion::GetCall(nsISOAPCall * *aCall)
{
  *aCall = mCall;
  NS_IF_ADDREF(*aCall);
  return NS_OK;
}

/* readonly attribute nsISOAPResponse response; */
NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::GetResponse(nsISOAPResponse *
					       *aResponse)
{
  *aResponse =
      mRequest ? (nsCOMPtr < nsISOAPResponse >) nsnull : mResponse;
  NS_IF_ADDREF(*aResponse);
  return NS_OK;
}

/* readonly attribute nsISOAPResponseListener listener; */
NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::GetListener(nsISOAPResponseListener *
					       *aListener)
{
  *aListener = mListener;
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

/* readonly attribute boolean isComplete; */
NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::GetIsComplete(PRBool * aIsComplete)
{
  *aIsComplete = mRequest == nsnull;
  return NS_OK;
}

/* boolean abort (); */
NS_IMETHODIMP nsHTTPSOAPTransportCompletion::Abort(PRBool * _retval)
{
  if (mRequest) {
    if (NS_SUCCEEDED(mRequest->Abort())) {
      *_retval = PR_TRUE;
      mRequest = nsnull;
      return NS_OK;
    }
  }
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::HandleEvent(nsIDOMEvent * aEvent)
{
//  PRUint32 status;
  nsresult rv = NS_OK;
  if (mRequest) {		//  Avoid if it has been aborted.
#if 0
    rv = mRequest->GetStatus(&status);
    if (NS_SUCCEEDED(rv) && (status < 200 || status >= 300))
      rv = NS_ERROR_FAILURE;
#endif
    if (mResponse) { // && NS_SUCCEEDED(rv)) {
      nsCOMPtr < nsIDOMDocument > document;
      rv = mRequest->GetResponseXML(getter_AddRefs(document));
      if (NS_SUCCEEDED(rv) && document) {
	rv = mResponse->SetMessage(document);
      DEBUG_DUMP_DOCUMENT("Asynchronous Response", document)} else {
	mResponse = nsnull;
      }
    } else {
      mResponse = nsnull;
    }
    nsCOMPtr < nsISOAPCallCompletion > kungFuDeathGrip = this;
    mRequest = nsnull;		//  Break cycle of references by releas.
    PRBool c;			//  In other transports, this may signal to stop returning if multiple returns
    mListener->HandleResponse(mResponse, mCall, rv, PR_TRUE, &c);
  }
  return NS_OK;
}

/* void asyncCall (in nsISOAPCall aCall, in nsISOAPResponseListener aListener, in nsISOAPResponse aResponse); */
NS_IMETHODIMP
    nsHTTPSOAPTransport::AsyncCall(nsISOAPCall * aCall,
				   nsISOAPResponseListener * aListener,
				   nsISOAPResponse * aResponse,
				   nsISOAPCallCompletion ** aCompletion)
{
  NS_ENSURE_ARG(aCall);

  nsresult rv;
  nsCOMPtr < nsIXMLHttpRequest > request;

  nsCOMPtr < nsIDOMDocument > messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv))
    return rv;
  if (!messageDocument)
    return NS_ERROR_NOT_INITIALIZED;

  DEBUG_DUMP_DOCUMENT("Asynchronous Request", messageDocument)
      request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr < nsIDOMEventTarget > eventTarget =
      do_QueryInterface(request, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString uri;
  rv = aCall->GetTransportURI(uri);
  if (NS_FAILED(rv))
    return rv;
  if (AStringIsNull(uri))
    return NS_ERROR_NOT_INITIALIZED;

  rv = request->OpenRequest("POST", NS_ConvertUCS2toUTF8(uri).get(),
			    PR_TRUE, nsnull, nsnull);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString action;
  rv = aCall->GetActionURI(action);
  if (NS_FAILED(rv))
    return rv;
  if (!AStringIsNull(action)) {
    rv = request->SetRequestHeader("SOAPAction",
				   NS_ConvertUCS2toUTF8(action).get());
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr < nsIWritableVariant > variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = variant->SetAsInterface(NS_GET_IID(nsIDOMDocument),
			       messageDocument);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr < nsISOAPCallCompletion > completion;

  if (aListener) {
    completion =
	new nsHTTPSOAPTransportCompletion(aCall, aResponse, request,
					  aListener);
    if (!completion)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr < nsIDOMEventListener > listener =
	do_QueryInterface(completion);
    rv = eventTarget->AddEventListener(NS_LITERAL_STRING("load"), listener,
				       PR_FALSE);
    if (NS_FAILED(rv))
      return rv;
    rv = eventTarget->AddEventListener(NS_LITERAL_STRING("error"),
				       listener, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;
  }
  rv = request->Send(variant);
  if (NS_FAILED(rv))
    return rv;

  *aCompletion = completion;
  NS_IF_ADDREF(*aCompletion);

  return NS_OK;
}

/* void addListener (in nsISOAPTransportListener aListener, in boolean aCapture); */
NS_IMETHODIMP
    nsHTTPSOAPTransport::AddListener(nsISOAPTransportListener * aListener,
				     PRBool aCapture)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeListener (in nsISOAPTransportListener aListener, in boolean aCapture); */
NS_IMETHODIMP
    nsHTTPSOAPTransport::RemoveListener(nsISOAPTransportListener *
					aListener, PRBool aCapture)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsHTTPSSOAPTransport::nsHTTPSSOAPTransport()
{
  NS_INIT_ISUPPORTS();
}

nsHTTPSSOAPTransport::~nsHTTPSSOAPTransport()
{
}

NS_IMPL_ISUPPORTS1_CI(nsHTTPSSOAPTransport, nsISupports)

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#include "nsXPIDLString.h"
#include "nsHTTPSOAPTransport.h"
#include "nsIComponentManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIVariant.h"
#include "nsString.h"
#include "nsSOAPUtils.h"
#include "nsSOAPCall.h"
#include "nsSOAPException.h"
#include "nsSOAPResponse.h"
#include "nsISOAPCallCompletion.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMSerializer.h"
#include "nsMemory.h"

nsHTTPSOAPTransport::nsHTTPSOAPTransport()
{
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

static NS_NAMED_LITERAL_STRING(kAnyURISchemaType, "anyURI");

/**
 * Get and check the transport URI for accessibility.  In the future,
 * this might also attempt to automatically add a mustUnderstand
 * header to messages for untrusted sources and send them anyway.
 */
static nsresult GetTransportURI(nsISOAPCall * aCall, nsAString & aURI)
{
  nsresult rc = aCall->GetTransportURI(aURI);
  if (NS_FAILED(rc))
    return rc;
  
  // Create a new URI
  nsCOMPtr<nsIURI> uri;
  rc = NS_NewURI(getter_AddRefs(uri), aURI, nsnull);
  if (NS_FAILED(rc)) return rc;

  // Get security manager, check to see if we're allowed to call this URI.
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rc);
  if (NS_FAILED(rc)) 
    return rc;
  PRBool safe = PR_FALSE;
  rc = aCall->GetVerifySourceHeader(&safe);
  if (NS_FAILED(rc)) 
    return rc;
  if (!safe) {
    if (NS_FAILED(secMan->CheckConnect(nsnull, uri, "SOAPCall", "invoke")))
      return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_INVOKE_DISABLED", "SOAPCall.invoke not enabled by client");
  }
  else {
    if (NS_FAILED(secMan->CheckConnect(nsnull, uri, "SOAPCall", "invokeVerifySourceHeader")))
      return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_INVOKE_VERIFY_DISABLED", "SOAPCall.invokeVerifySourceHeader not enabled by client");

    nsAutoString sourceURI;

    {

      nsCOMPtr<nsIPrincipal> principal;
      rc = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
      if (NS_FAILED(rc)) 
        return rc;
      if (!principal) {
        return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_INVOKE_VERIFY_PRINCIPAL", "Source-verified message cannot be sent without principal.");
      }
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal,&rc);
      if (NS_FAILED(rc)) 
        return rc;
  
      if (!codebase) {
        return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_INVOKE_VERIFY_CODEBASE", "Source-verified message cannot be sent without codebase.");
      }
  
      char* str;

      rc = codebase->GetSpec(&str);
      if (NS_FAILED(rc)) 
        return rc;
      CopyASCIItoUCS2(nsDependentCString(str), sourceURI);
      nsMemory::Free(str);

    }

//  Adding a header to tell the server that it must understand and verify the source of the call

    nsCOMPtr<nsIDOMElement> element;
    rc = aCall->GetHeader(getter_AddRefs(element));
    if (NS_FAILED(rc)) 
      return rc;
    if (!element)
      return SOAP_EXCEPTION(NS_ERROR_FAILURE,"SOAP_INVOKE_VERIFY_HEADER", "Source-verified message cannot be sent without a header.");
    //  Node ignored on remove / append calls
    nsCOMPtr<nsIDOMNode> ignore;
    //  Remove any existing elements that may conflict with this verifySource identification
    nsCOMPtr<nsIDOMElement> verifySource;
    for (;;) {
      nsSOAPUtils::GetSpecificChildElement(nsnull, element, nsSOAPUtils::kVerifySourceNamespaceURI, 
        nsSOAPUtils::kVerifySourceHeader, getter_AddRefs(verifySource));
      if (verifySource) {
        element->RemoveChild(verifySource, getter_AddRefs(ignore));
        if (NS_FAILED(rc))
          return rc;
      }
      else
       break;
    }
    //  Document as factory
    nsCOMPtr<nsIDOMDocument> document;
    rc = element->GetOwnerDocument(getter_AddRefs(document));
    if (NS_FAILED(rc)) 
      return rc;
    //  Proper version to use of SOAP
    PRUint16 version;
    rc = aCall->GetVersion(&version);
    if (NS_FAILED(rc)) 
      return rc;
    //  Proper schema to use for types
    nsAutoString XSURI;
    nsAutoString XSIURI;
    nsAutoString SOAPEncURI;
    if (version == nsISOAPMessage::VERSION_1_1) {
      XSURI.Assign(nsSOAPUtils::kXSURI1999);
      XSIURI.Assign(nsSOAPUtils::kXSIURI1999);
      SOAPEncURI.Assign(nsSOAPUtils::kSOAPEncURI11);
    }
    else {
      XSURI.Assign(nsSOAPUtils::kXSURI);
      XSIURI.Assign(nsSOAPUtils::kXSIURI);
      SOAPEncURI.Assign(nsSOAPUtils::kSOAPEncURI);
    }
    //  Create the header and append it with mustUnderstand and normal encoding.
    rc = document->CreateElementNS(nsSOAPUtils::kVerifySourceNamespaceURI, 
      nsSOAPUtils::kVerifySourceHeader, 
      getter_AddRefs(verifySource));
    if (NS_FAILED(rc)) 
      return rc;
    rc = element->AppendChild(verifySource, getter_AddRefs(ignore));
    if (NS_FAILED(rc)) 
      return rc;
    rc = verifySource->SetAttributeNS(*nsSOAPUtils::kSOAPEnvURI[version],
      nsSOAPUtils::kMustUnderstandAttribute,nsSOAPUtils::kTrueA);// mustUnderstand
    if (NS_FAILED(rc)) 
      return rc;
    rc = verifySource->SetAttributeNS(*nsSOAPUtils::kSOAPEnvURI[version], 
      nsSOAPUtils::kEncodingStyleAttribute, SOAPEncURI);// 1.2 encoding
    if (NS_FAILED(rc)) 
      return rc;

    //  Prefixed string for xsi:string
    nsAutoString stringType;
    {
      nsAutoString prefix;
      rc = nsSOAPUtils::MakeNamespacePrefix(nsnull, verifySource, XSURI, stringType);
      if (NS_FAILED(rc)) 
        return rc;
      stringType.Append(nsSOAPUtils::kQualifiedSeparator);
      stringType.Append(kAnyURISchemaType);
    }

    //  If it is available, add the sourceURI 
    if (!sourceURI.IsEmpty()) {
      rc = document->CreateElementNS(nsSOAPUtils::kVerifySourceNamespaceURI,
        nsSOAPUtils::kVerifySourceURI,getter_AddRefs(element));
      if (NS_FAILED(rc)) 
        return rc;
      rc = verifySource->AppendChild(element, getter_AddRefs(ignore));
      if (NS_FAILED(rc)) 
        return rc;
      rc = element->SetAttributeNS(XSIURI,
        nsSOAPUtils::kXSITypeAttribute,stringType);
      if (NS_FAILED(rc)) 
        return rc;
      nsCOMPtr<nsIDOMText> text;
      rc = document->CreateTextNode(sourceURI, getter_AddRefs(text));
      if (NS_FAILED(rc)) 
      return rc;
      element->AppendChild(text, getter_AddRefs(ignore));
      if (NS_FAILED(rc)) 
      return rc;
    }
  }
  return NS_OK;
}

/* void syncCall (in nsISOAPCall aCall, in nsISOAPResponse aResponse); */
NS_IMETHODIMP nsHTTPSOAPTransport::SyncCall(nsISOAPCall * aCall, nsISOAPResponse * aResponse)
{
  NS_ENSURE_ARG(aCall);

  nsresult rv;
  nsCOMPtr < nsIXMLHttpRequest > request;

  nsCOMPtr < nsIDOMDocument > messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv))
    return rv;
  if (!messageDocument)
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_MESSAGE_DOCUMENT", "No message document is present.");

  nsAutoString uri;
  rv = GetTransportURI(aCall, uri);
  if (NS_FAILED(rv))
    return rv;
  if (AStringIsNull(uri))
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_TRANSPORT_URI", "No transport URI was specified.");

  DEBUG_DUMP_DOCUMENT("Synchronous Request", messageDocument)

  request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = request->OverrideMimeType("text/xml");
  if (NS_FAILED(rv))
    return rv;
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
}

nsHTTPSOAPTransportCompletion::nsHTTPSOAPTransportCompletion(nsISOAPCall * call, nsISOAPResponse * response, nsIXMLHttpRequest * request, nsISOAPResponseListener * listener) :
mCall(call), mResponse(response), mRequest(request), mListener(listener)
{
}

nsHTTPSOAPTransportCompletion::~nsHTTPSOAPTransportCompletion()
{
}

/* readonly attribute nsISOAPCall call; */
NS_IMETHODIMP nsHTTPSOAPTransportCompletion::GetCall(nsISOAPCall * *aCall)
{
  NS_ENSURE_ARG(aCall);
  *aCall = mCall;
  NS_IF_ADDREF(*aCall);
  return NS_OK;
}

/* readonly attribute nsISOAPResponse response; */
NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::GetResponse(nsISOAPResponse *
                                               *aResponse)
{
  NS_ENSURE_ARG(aResponse);
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
  NS_ENSURE_ARG(aListener);
  *aListener = mListener;
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

/* readonly attribute boolean isComplete; */
NS_IMETHODIMP
    nsHTTPSOAPTransportCompletion::GetIsComplete(PRBool * aIsComplete)
{
  NS_ENSURE_ARG(aIsComplete);
  *aIsComplete = mRequest == nsnull;
  return NS_OK;
}

/* boolean abort (); */
NS_IMETHODIMP nsHTTPSOAPTransportCompletion::Abort(PRBool * _retval)
{
  NS_ENSURE_ARG(_retval);
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
  NS_ENSURE_ARG(aEvent);
//  PRUint32 status;
  nsresult rv = NS_OK;
  if (mRequest) {                //  Avoid if it has been aborted.
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
    mRequest = nsnull;                //  Break cycle of references by releas.
    PRBool c;                        //  In other transports, this may signal to stop returning if multiple returns
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
  NS_ENSURE_ARG(aCompletion);

  nsresult rv;
  nsCOMPtr < nsIXMLHttpRequest > request;

  nsCOMPtr < nsIDOMDocument > messageDocument;
  rv = aCall->GetMessage(getter_AddRefs(messageDocument));
  if (NS_FAILED(rv))
    return rv;
  if (!messageDocument)
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_MESSAGE_DOCUMENT", "No message document is present.");

  request = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr < nsIDOMEventTarget > eventTarget =
      do_QueryInterface(request, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString uri;
  rv = GetTransportURI(aCall, uri);
  if (NS_FAILED(rv))
    return rv;
  if (AStringIsNull(uri))
    return SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_TRANSPORT_URI", "No transport URI was specified.");

  DEBUG_DUMP_DOCUMENT("Asynchronous Request", messageDocument)
  rv = request->OverrideMimeType("text/xml");
  if (NS_FAILED(rv))
    return rv;
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
}

nsHTTPSSOAPTransport::~nsHTTPSSOAPTransport()
{
}

NS_IMPL_ISUPPORTS1_CI(nsHTTPSSOAPTransport, nsISOAPTransport)

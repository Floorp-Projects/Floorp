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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsSOAPCall.h"
#include "nsSOAPResponse.h"
#include "nsSOAPParameter.h"
#include "nsSOAPUtils.h"
#include "nsCRT.h"
#include "jsapi.h"
#include "nsIDOMParser.h"
#include "nsISOAPEncoder.h"
#include "nsISOAPParameter.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kDOMParserCID, NS_DOMPARSER_CID);

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

class nsScriptResponseListener : public nsISOAPResponseListener 
{
public:
  nsScriptResponseListener(JSObject* aScopeObj, JSObject* aFunctionObj);
  virtual ~nsScriptResponseListener();
  
  NS_DECL_ISUPPORTS

  // nsISOAPResponseListener
  NS_DECL_NSISOAPRESPONSELISTENER

protected:  
  JSObject* mScopeObj;
  JSObject* mFunctionObj;
};

nsScriptResponseListener::nsScriptResponseListener(JSObject* aScopeObj,
                                                   JSObject* aFunctionObj)
{
  NS_INIT_ISUPPORTS();
  // We don't have to add a GC root for the scope object
  // since we'll go away if it goes away
  mScopeObj = aScopeObj;
  mFunctionObj = aFunctionObj;
  JSContext* cx;
  cx = nsSOAPUtils::GetSafeContext();
  if (cx) {
    JS_AddNamedRoot(cx, &mFunctionObj, "nsSOAPCall");
  }
}

nsScriptResponseListener::~nsScriptResponseListener()
{
  JSContext* cx;
  cx = nsSOAPUtils::GetSafeContext();
  if (cx) {
    JS_RemoveRoot(cx, &mFunctionObj);
  }
}

NS_IMPL_ISUPPORTS1(nsScriptResponseListener,
                   nsISOAPResponseListener)

NS_IMETHODIMP
nsScriptResponseListener::HandleResponse(nsISOAPResponse* aResponse,
                                         nsISOAPCall* aCall,
                                         PRUint32 status)
{
  nsresult rv;
  JSContext* cx;
  cx = nsSOAPUtils::GetCurrentContext();
  if (!cx) {
    cx = nsSOAPUtils::GetSafeContext();
  }
  if (cx) {
    nsCOMPtr<nsIXPConnect> xpc =
      do_GetService(nsIXPConnect::GetCID()); 
    if (!xpc) return NS_OK;

    jsval params[3];
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JSObject* obj;

    // Get the JSObject wrapper for the response
    rv = xpc->WrapNative(cx, mScopeObj,
                         aResponse, NS_GET_IID(nsISOAPResponse),
                         getter_AddRefs(holder));
    if (NS_FAILED(rv)) return NS_OK;

    rv = holder->GetJSObject(&obj);
    if (!obj) return NS_OK;

    params[0] = OBJECT_TO_JSVAL(obj);

    // Get the JSObject wrapper for the call
    rv = xpc->WrapNative(cx, mScopeObj,
                         aCall, NS_GET_IID(nsISOAPCall),
                         getter_AddRefs(holder));
    if (NS_FAILED(rv)) return NS_OK;

    rv = holder->GetJSObject(&obj);
    if (!obj) return NS_OK;

    params[1] = OBJECT_TO_JSVAL(obj);

    params[2] = INT_TO_JSVAL(status);

    jsval val;
    JS_CallFunctionValue(cx, mScopeObj, OBJECT_TO_JSVAL(mFunctionObj),
                         3, params, &val);
  }

  return NS_OK;
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsSOAPCall::nsSOAPCall()
{
  NS_INIT_ISUPPORTS();
  mStatus = 0;
}

nsSOAPCall::~nsSOAPCall()
{
}

NS_IMPL_ISUPPORTS3(nsSOAPCall, 
                   nsISOAPCall, 
                   nsISecurityCheckedComponent,
                   nsISOAPTransportListener)


static const char* kEmptySOAPDocStr = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">"
"<SOAP-ENV:Header>"
"</SOAP-ENV:Header>"
"<SOAP-ENV:Body>"
"</SOAP-ENV:Body>"
"</SOAP-ENV:Envelope>";

nsresult
nsSOAPCall::EnsureDocumentAllocated()
{
  if (!mEnvelopeDocument) {
    nsresult rv;

    nsCOMPtr<nsIDOMParser> parser = do_CreateInstance(kDOMParserCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsAutoString docstr;
    docstr.AssignWithConversion(kEmptySOAPDocStr);
    rv = parser->ParseFromString(docstr.get(), "text/xml", 
                                 getter_AddRefs(mEnvelopeDocument));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    mEnvelopeDocument->GetDocumentElement(getter_AddRefs(mEnvelopeElement));
    if (!mEnvelopeElement) return NS_ERROR_FAILURE;

    nsSOAPUtils::GetFirstChildElement(mEnvelopeElement, 
                                      getter_AddRefs(mHeaderElement));
    if (!mHeaderElement) return NS_ERROR_FAILURE;
    
    nsSOAPUtils::GetNextSiblingElement(mHeaderElement, 
                                       getter_AddRefs(mBodyElement));
    if (!mBodyElement) return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/* readonly attribute nsIDOMElement envelope; */
NS_IMETHODIMP nsSOAPCall::GetEnvelope(nsIDOMElement * *aEnvelope)
{
  NS_ENSURE_ARG_POINTER(aEnvelope);

  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;
  
  *aEnvelope = mEnvelopeElement;
  NS_ADDREF(*aEnvelope);

  return NS_OK;
}

/* readonly attribute nsIDOMElement header; */
NS_IMETHODIMP nsSOAPCall::GetHeader(nsIDOMElement * *aHeader)
{
  NS_ENSURE_ARG_POINTER(aHeader);

  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;
  
  *aHeader = mHeaderElement;
  NS_ADDREF(*aHeader);

  return NS_OK;
}

/* readonly attribute nsIDOMElement body; */
NS_IMETHODIMP nsSOAPCall::GetBody(nsIDOMElement * *aBody)
{
  NS_ENSURE_ARG_POINTER(aBody);

  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;
  
  *aBody = mBodyElement;
  NS_ADDREF(*aBody);

  return NS_OK;
}

/* attribute string encodingStyleURI; */
NS_IMETHODIMP nsSOAPCall::GetEncodingStyleURI(char * *aEncodingStyleURI)
{
  NS_ENSURE_ARG_POINTER(aEncodingStyleURI);

  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString value;
  rv = mEnvelopeElement->GetAttributeNS(NS_ConvertASCIItoUCS2(nsSOAPUtils::kSOAPEnvURI), 
                                        NS_ConvertASCIItoUCS2(nsSOAPUtils::kEncodingStyleAttribute),
                                        value);

  if (value.Length() > 0) {
    *aEncodingStyleURI = ToNewCString(value);
    if (nsnull == *aEncodingStyleURI) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    *aEncodingStyleURI = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP nsSOAPCall::SetEncodingStyleURI(const char * aEncodingStyleURI)
{
  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;
 
  if (nsnull == aEncodingStyleURI) {
    mEnvelopeElement->RemoveAttributeNS(NS_ConvertASCIItoUCS2(nsSOAPUtils::kSOAPEnvURI), 
                                        NS_ConvertASCIItoUCS2(nsSOAPUtils::kEncodingStyleAttribute));
  }
  else {
    mEnvelopeElement->SetAttributeNS(NS_ConvertASCIItoUCS2(nsSOAPUtils::kSOAPEnvURI), 
                                     NS_ConvertASCIItoUCS2(nsSOAPUtils::kEncodingStyleAttribute), 
                                     NS_ConvertASCIItoUCS2(aEncodingStyleURI));
  }

  return NS_OK;
}

PRBool
nsSOAPCall::HasBodyEntry()
{
  if (!mBodyElement) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMElement> entry;
  nsSOAPUtils::GetFirstChildElement(mBodyElement, getter_AddRefs(entry));
  
  if (entry) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

nsresult
nsSOAPCall::CreateBodyEntry(PRBool aNewParameters)
{
  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;

  // Create the element that will be the new body entry
  nsCOMPtr<nsIDOMElement> entry;
  nsCOMPtr<nsIDOMNode> dummy;
  
  rv = mEnvelopeDocument->CreateElementNS(NS_ConvertASCIItoUCS2(mTargetObjectURI.get()), 
                                          mMethodName, getter_AddRefs(entry));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // See if there's an existing body entry (we only worry
  // about the first).
  nsCOMPtr<nsIDOMElement> oldEntry;
  nsSOAPUtils::GetFirstChildElement(mBodyElement, getter_AddRefs(oldEntry));

  // If there is, we're going to replace it, but preserve its
  // children.
  if (oldEntry) {
    // Remove the old entry from the body
    mBodyElement->RemoveChild(oldEntry, getter_AddRefs(dummy));
    
    if (!aNewParameters) {
      // Transfer the children from the old to the new
      nsCOMPtr<nsIDOMNode> child;
      oldEntry->GetFirstChild(getter_AddRefs(child));
      while (child) {
        oldEntry->RemoveChild(child, getter_AddRefs(dummy));
        entry->AppendChild(child, getter_AddRefs(dummy));
        
        nsCOMPtr<nsIDOMNode> temp = child;
        temp->GetNextSibling(getter_AddRefs(child));
      }
    }
  }

  mBodyElement->AppendChild(entry, getter_AddRefs(dummy));

  // If there wasn't an old entry and we have parameters, or we
  // we have new parameters, create the parameter elements.
  if ((!entry && mParameters) || aNewParameters) {
    rv = CreateParameterElements();
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

/* attribute string targetObjectURI; */
NS_IMETHODIMP nsSOAPCall::GetTargetObjectURI(char * *aTargetObjectURI)
{
  NS_ENSURE_ARG_POINTER(aTargetObjectURI);
  
  if (mTargetObjectURI.Length() > 0) {
    *aTargetObjectURI = ToNewCString(mTargetObjectURI);
  }
  else {
    *aTargetObjectURI = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPCall::SetTargetObjectURI(const char * aTargetObjectURI)
{
  NS_ENSURE_ARG(aTargetObjectURI);

  mTargetObjectURI.Assign(aTargetObjectURI);
  if ((mTargetObjectURI.Length() > 0) && (mMethodName.Length() > 0)) {
    return CreateBodyEntry(PR_FALSE);
  }

  return NS_OK;
}

/* attribute string methodName; */
NS_IMETHODIMP nsSOAPCall::GetMethodName(PRUnichar * *aMethodName)
{
  NS_ENSURE_ARG_POINTER(aMethodName);
  
  if (mMethodName.Length() > 0) {
    *aMethodName = ToNewUnicode(mMethodName);
  }
  else {
    *aMethodName = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPCall::SetMethodName(const PRUnichar * aMethodName)
{
  NS_ENSURE_ARG(aMethodName);
  
  mMethodName.Assign(aMethodName);
  if ((mTargetObjectURI.Length() > 0) && (mMethodName.Length() > 0)) {
    return CreateBodyEntry(PR_FALSE);
  }

  return NS_OK;
}

/* attribute string destinationURI; */
NS_IMETHODIMP nsSOAPCall::GetDestinationURI(char * *aDestinationURI)
{
  NS_ENSURE_ARG_POINTER(aDestinationURI);
  
  if (mDestinationURI.Length() > 0) {
    *aDestinationURI = ToNewCString(mDestinationURI);
  }
  else {
    *aDestinationURI = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPCall::SetDestinationURI(const char * aDestinationURI)
{
  if (aDestinationURI) {
    mDestinationURI.Assign(aDestinationURI);
  }
  else {
    mDestinationURI.Truncate();
  }

  return NS_OK;
}

/* attribute string actionURI; */
NS_IMETHODIMP nsSOAPCall::GetActionURI(char * *aActionURI)
{
  NS_ENSURE_ARG_POINTER(aActionURI);
  
  if (mActionURI.Length() > 0) {
    *aActionURI = ToNewCString(mActionURI);
  }
  else {
    *aActionURI = nsnull;
  }

  return NS_OK;
}
NS_IMETHODIMP nsSOAPCall::SetActionURI(const char * aActionURI)
{
  if (aActionURI) {
    mActionURI.Assign(aActionURI);
  }
  else {
    mActionURI.Truncate();
  }

  return NS_OK;
}

nsresult
nsSOAPCall::CreateParameterElements()
{
  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;

  // Get the body entry that's going to be the parent of
  // the parameter elements. If we got here, there should
  // be one.
  nsCOMPtr<nsIDOMElement> entry;
  nsSOAPUtils::GetFirstChildElement(mBodyElement, getter_AddRefs(entry));
  if (!entry) return NS_ERROR_FAILURE;

  // Get the inherited encoding style starting from the
  // body entry.
  nsXPIDLCString encodingStyle;
  nsSOAPUtils::GetInheritedEncodingStyle(entry, getter_Copies(encodingStyle));

  // Find the corresponding encoder
  nsCAutoString encoderContractid;
  encoderContractid.Assign(NS_SOAPENCODER_CONTRACTID_PREFIX);
  encoderContractid.Append(encodingStyle);

  nsCOMPtr<nsISOAPEncoder> encoder = do_CreateInstance(encoderContractid.get());
  if (!encoder) return NS_ERROR_INVALID_ARG;

  PRUint32 index, count;
  mParameters->Count(&count);

  for(index = 0; index < count; index++) {
    nsCOMPtr<nsISupports> isup = getter_AddRefs(mParameters->ElementAt(index));
    nsCOMPtr<nsISOAPParameter> parameter = do_QueryInterface(isup);
    
    if (parameter) {
      nsCOMPtr<nsISOAPEncoder> paramEncoder = encoder;

      // See if the parameter has its own encoding style
      nsXPIDLCString paramEncoding;
      parameter->GetEncodingStyleURI(getter_Copies(paramEncoding));
      
      // If it does and it's different from the inherited one,
      // find an encoder
      if (paramEncoding && 
          (nsCRT::strcmp(encodingStyle, paramEncoding) != 0)) {
        nsCAutoString paramEncoderContractid;
        paramEncoderContractid.Assign(NS_SOAPENCODER_CONTRACTID_PREFIX);
        paramEncoderContractid.Append(paramEncoding);
        
        paramEncoder = do_CreateInstance(paramEncoderContractid.get());
        if (!paramEncoder) return NS_ERROR_INVALID_ARG;
      }

      // Convert the parameter to an element
      nsCOMPtr<nsIDOMElement> element;
      encoder->ParameterToElement(parameter,
                                  paramEncoding ? paramEncoding : encodingStyle,
                                  mEnvelopeDocument,
                                  getter_AddRefs(element));
      
      // Append the parameter element to the body entry
      nsCOMPtr<nsIDOMNode> dummy;
      entry->AppendChild(element, getter_AddRefs(dummy));
    }
  }
  
  return NS_OK;
}

nsresult
nsSOAPCall::ClearParameterElements()
{
  nsresult rv = EnsureDocumentAllocated();
  if (NS_FAILED(rv)) return rv;

  // Get the body entry that's the parent of the parameter
  // elements (assuming there is one)
  nsCOMPtr<nsIDOMElement> entry;
  nsSOAPUtils::GetFirstChildElement(mBodyElement, getter_AddRefs(entry));

  if (entry) {
    // Get rid of all the children of the body entry
    nsCOMPtr<nsIDOMNode> child;
    entry->GetFirstChild(getter_AddRefs(child));
    while (child) {
      nsCOMPtr<nsIDOMNode> dummy;
      entry->RemoveChild(child, getter_AddRefs(dummy));
      entry->GetFirstChild(getter_AddRefs(child));
    }
  }
  
  return NS_OK;
}

/* [noscript] void setSOAPParameters ([array, size_is (count)] in nsISOAPParameter parameters, in unsigned long count); */
NS_IMETHODIMP nsSOAPCall::SetSOAPParameters(nsISOAPParameter **parameters, PRUint32 count)
{
  nsresult rv;

  // Clear out any existing parameters
  if (mParameters) {
    ClearParameterElements();
    mParameters->Clear();
  }
  else {
    rv = NS_NewISupportsArray(getter_AddRefs(mParameters));
    if (!mParameters) return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 index;
  for (index = 0; index < count; index++) {
    nsISOAPParameter* parameter = parameters[index];
    if (parameter) {
      mParameters->AppendElement(parameter);
    }
  }

  if (HasBodyEntry()) {
    return CreateParameterElements();
  }
  else if ((mTargetObjectURI.Length() > 0) && (mMethodName.Length() > 0)) {
    return CreateBodyEntry(PR_TRUE);
  }

  return NS_OK;
}

/* void setParameters (); */
NS_IMETHODIMP nsSOAPCall::SetParameters()
{
  nsresult rv;

  // Clear out any existing parameters
  if (mParameters) {
    ClearParameterElements();
    mParameters->Clear();
  }
  else {
    rv = NS_NewISupportsArray(getter_AddRefs(mParameters));
    if (!mParameters) return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  // This should only be called from script
  if (NS_FAILED(rv) || !cc) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 argc;
  rv = cc->GetArgc(&argc);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  jsval* argv;
  rv = cc->GetArgvPtr(&argv);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  JSContext* cx;
  rv = cc->GetJSContext(&cx);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // For each parameter to this method
  PRUint32 index;
  for (index = 0; index < argc; index++) {
    nsCOMPtr<nsISOAPParameter> param;
    jsval val = argv[index];
    
    // First check if it's a parameter
    if (JSVAL_IS_OBJECT(val)) {
      JSObject* paramobj;
      paramobj = JSVAL_TO_OBJECT(val);

      // Check if it's a wrapped native
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      xpc->GetWrappedNativeOfJSObject(cx, paramobj, getter_AddRefs(wrapper));
      
      if (wrapper) {
        // Get the native and see if it's a SOAPParameter
        nsCOMPtr<nsISupports> native;
        wrapper->GetNative(getter_AddRefs(native));
        if (native) {
          param = do_QueryInterface(native);
        }
      }
    }

    // Otherwise create a new parameter with the value
    if (!param) {
      nsSOAPParameter* newparam = new nsSOAPParameter();
      if (!newparam) return NS_ERROR_OUT_OF_MEMORY;
      
      param = (nsISOAPParameter*)newparam;
      rv = newparam->SetValue(cx, val);
      if (NS_FAILED(rv)) return rv;
    }

    mParameters->AppendElement(param);
  }

  if (HasBodyEntry()) {
    return CreateParameterElements();
  }
  else if ((mTargetObjectURI.Length() > 0) && (mMethodName.Length() > 0)) {
    return CreateBodyEntry(PR_TRUE);
  }

  return NS_OK;
}

nsresult
nsSOAPCall::GetTransport(nsISOAPTransport** aTransport)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsXPIDLCString protocol;

  rv = NS_NewURI(getter_AddRefs(uri), mDestinationURI.get());
  if (NS_FAILED(rv)) return rv;

  uri->GetScheme(getter_Copies(protocol));
  
  nsCAutoString transportContractid;
  transportContractid.Assign(NS_SOAPTRANSPORT_CONTRACTID_PREFIX);
  transportContractid.Append(protocol);

  nsCOMPtr<nsISOAPTransport> transport = do_CreateInstance(transportContractid.get());
  if (!transport) return NS_ERROR_INVALID_ARG;

  *aTransport = transport.get();
  NS_ADDREF(*aTransport);

  return NS_OK;
}

/* nsISOAPResponse invoke (); */
NS_IMETHODIMP nsSOAPCall::Invoke(nsISOAPResponse **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsresult rv;
  nsCOMPtr<nsISOAPTransport> transport;

  if (mDestinationURI.Length() == 0) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  rv = GetTransport(getter_AddRefs(transport));
  if (NS_FAILED(rv)) return rv;

  PRBool canDoSync;
  transport->CanDoSync(&canDoSync);

  if (!canDoSync) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDOMDocument> responseDocument;
  rv = transport->SyncCall(mDestinationURI.get(),
                           mActionURI.get(),
                           mEnvelopeDocument,
                           getter_AddRefs(responseDocument));
  if (NS_FAILED(rv)) return rv;

  transport->GetStatus(&mStatus);

  nsSOAPResponse* response;
  response = new nsSOAPResponse(responseDocument);
  if (!response) return NS_ERROR_OUT_OF_MEMORY;

  response->SetStatus(mStatus);

  return response->QueryInterface(NS_GET_IID(nsISOAPResponse), (void**)_retval);
}

nsresult
nsSOAPCall::GetScriptListener(nsISupports* aObject,
                              nsISOAPResponseListener** aListener)
{
  nsresult rv;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> jsobjholder = do_QueryInterface(aObject);
    if (jsobjholder) {
      JSObject* funobj;
      rv = jsobjholder->GetJSObject(&funobj);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      JSContext* cx;
      rv = cc->GetJSContext(&cx);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
      JSFunction* fun = JS_ValueToFunction(cx, OBJECT_TO_JSVAL(funobj));
      if (!fun) {
        return NS_ERROR_INVALID_ARG;
      }
      
      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      rv = cc->GetCalleeWrapper(getter_AddRefs(wrapper));
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      JSObject* scopeobj;
      rv = wrapper->GetJSObject(&scopeobj);
      if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

      nsScriptResponseListener* listener = new nsScriptResponseListener(scopeobj, funobj);
      if (!listener) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *aListener = listener;
      NS_ADDREF(*aListener);
    }
  }
  
  return NS_OK;
}

/* void asyncInvoke (in nsISupports listener); */
NS_IMETHODIMP nsSOAPCall::AsyncInvoke(nsISupports *listener)
{
  nsresult rv;
  nsCOMPtr<nsISOAPTransport> transport;

  if (mDestinationURI.Length() == 0) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  rv = GetTransport(getter_AddRefs(transport));
  if (NS_FAILED(rv)) return rv;
  
  mListener = do_QueryInterface(listener);
  // We first try to do a direct QI, if that doesn't work
  // maybe it's a script event listener
  if (!mListener) {
    rv = GetScriptListener(listener, getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;
  }

  rv = transport->AsyncCall(mDestinationURI.get(),
                            mActionURI.get(),
                            mEnvelopeDocument,
                            this);
  return rv;
}

NS_IMETHODIMP
nsSOAPCall::GetStatus(PRUint32 *aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);

  *aStatus = mStatus;
  return NS_OK;
}

/* void handleResponse (in nsIDOMDocument document, in unsigned long status); */
NS_IMETHODIMP nsSOAPCall::HandleResponse(nsIDOMDocument *document, PRUint32 status, nsresult result)
{
  mStatus = status;
  if (mListener) {
    nsCOMPtr<nsISOAPResponse> response;
    if (NS_SUCCEEDED(result)) {
      nsSOAPResponse* respobj;
      respobj = new nsSOAPResponse(document);
      if (!respobj) result = NS_ERROR_OUT_OF_MEMORY;

      respobj->SetStatus(status);

      response = NS_STATIC_CAST(nsISOAPResponse*, respobj);
    }

    mListener->HandleResponse(response,
                              this,
                              status);
  }
  
  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPCall::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPCall))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPCall::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPCall))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPCall::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPCall))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPCall::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPCall))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

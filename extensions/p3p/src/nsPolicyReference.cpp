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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

#include "nsPolicyReference.h"
#include "nsNetUtil.h"
#include "nsP3PUtils.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIPolicyListener.h"

#define CLEAN_ARRAY_AND_RETURN_IF_FAILED(_result, _array) \
 PR_BEGIN_MACRO                    \
 if(NS_FAILED(_result)) {          \
   nsP3PUtils::CleanArray(_array); \
   return _result;                 \
 }                                 \
 PR_END_MACRO

static const char kWellKnownLocation[] = "/w3c/p3p.xml";
static const char kW3C[] = "/w3c/";

static nsresult
RequestSucceeded(nsIXMLHttpRequest* aRequest, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aRequest);

  nsresult rv;
  nsCOMPtr<nsIChannel> channel;
  aRequest->GetChannel(getter_AddRefs(channel));
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel, &rv));
  NS_ENSURE_TRUE(httpChannel, rv);

  return httpChannel->GetRequestSucceeded(aReturn);
}

NS_IMPL_ISUPPORTS4(nsPolicyReference,
                   nsIPolicyReference,
                   nsIPolicyTarget,
                   nsIDOMEventListener,
                   nsISupportsWeakReference)

nsPolicyReference::nsPolicyReference() 
  : mFlags (0),
    mError (0)
{
}

nsPolicyReference::~nsPolicyReference()
{
}

NS_IMETHODIMP
nsPolicyReference::Initialize(nsIURI* aMainURI)
{
  NS_ENSURE_ARG_POINTER(aMainURI);

  mMainURI = aMainURI;
  mFlags = 0;
  mError = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsPolicyReference::Finalize() 
{
  nsresult result = NS_OK;
  if (mXMLHttpRequest) { 
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mXMLHttpRequest));
    if (target) {
      result = target->RemoveEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
    }
  }
  return result;
}

NS_IMETHODIMP
nsPolicyReference::HandleEvent(nsIDOMEvent  *aEvent) 
{
  NS_ASSERTION(mXMLHttpRequest, "no xml http request");

  nsresult result = NS_OK;
  nsCOMPtr<nsIPolicyListener> listener(do_QueryReferent(mListener));
  NS_ENSURE_TRUE(listener, NS_ERROR_FAILURE);

  if (mXMLHttpRequest) {
    nsCOMPtr<nsIDOMDocument> document;
    if (mFlags & IS_MAIN_URI) {
      if (!mDocument) {
        mXMLHttpRequest->GetResponseXML(getter_AddRefs(mDocument));
        PRBool success;
        result = RequestSucceeded(mXMLHttpRequest, &success);
        if (NS_FAILED(result) || !success) {
          listener->NotifyPolicyLocation(0, POLICY_LOAD_FAILURE);
          return result;
        }
      }
      document = mDocument;
    }
    else {
      mXMLHttpRequest->GetResponseXML(getter_AddRefs(document));
      PRBool success;
      result = RequestSucceeded(mXMLHttpRequest, &success);
      if (NS_FAILED(result) || !success) {
        listener->NotifyPolicyLocation(0, POLICY_LOAD_FAILURE);
        return result;
      }
      if (mFlags & IS_LINKED_URI) {
        mDocument = document;
      }
    }
  
    nsXPIDLCString policyLocation;
    result = ProcessPolicyReferenceFile(document, getter_Copies(policyLocation));
      
    if (NS_SUCCEEDED(result)) {
      listener->NotifyPolicyLocation(policyLocation, mError);
    }
    else {
      listener->NotifyPolicyLocation(0, POLICY_LOAD_FAILURE);
    }
  }

  return result;
}

/** Call this method to load policy reference file for a given URI
  * 
  * @param aFlag  - Describes the state aURI. Possible values are:
  * IS_EMBEDDED_URI => aURI is different from the  main URI and therefore we 
  *                    we should first try to load the main URI's policy.
  * IS_MAIN_URI     => Load policy for this URI from its well known location.
  * IS_LINKED_URI   => We did not find a policy in well known location of the 
  *                    selected/main URI and therefore load the URI referenced 
  *                    by the LINK tag in the current document.
  **/
NS_IMETHODIMP
nsPolicyReference::LoadPolicyReferenceFileFor(nsIURI* aURI,
                                              PRUint32 aFlag)
{

  NS_ENSURE_ARG_POINTER(aURI);

  nsresult result = NS_OK;
  
  mFlags      = aFlag;
  mCurrentURI = aURI;

  if (mFlags & IS_MAIN_URI) {
    // Load policy reference file from the well known location.
    // well known location = /w3c/p3p.xml
    if (!mDocument) {
      nsXPIDLCString value;
      mMainURI->GetPrePath(value);
      value += NS_LITERAL_CSTRING(kWellKnownLocation);
      result = Load(value);
    }
    else {
      // Policy reference document already exists.
      result = HandleEvent(0);
    }
  }
  else if (mFlags & IS_EMBEDDED_URI) {
    // The embedded uri's host is different from the main uri's host. 
    // Load the policy reference file from the embedded uri's 
    // well known location
    nsXPIDLCString value;
    mCurrentURI->GetPrePath(value);
    value += NS_LITERAL_CSTRING(kWellKnownLocation);
    result = Load(value);
  }
  else if (mFlags & IS_LINKED_URI) {
    // Load policy referenced via html/xhtml LINK tag.
    mLinkedURI = aURI;
    nsXPIDLCString value;
    mLinkedURI->GetSpec(value);
    result = Load(value);
  }
 
  return result;
}

NS_IMETHODIMP
nsPolicyReference::SetupPolicyListener(nsIPolicyListener* aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);

  mListener = do_GetWeakReference(aListener);
  return NS_OK;
}

nsresult
nsPolicyReference::Load(const nsACString& aURI)
{
  NS_ASSERTION(aURI.Length(), "no uri to load");

  nsresult result = NS_OK;

  if (!mXMLHttpRequest) {
    mXMLHttpRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &result);
    NS_ENSURE_SUCCESS(result, result); 
    
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mXMLHttpRequest, &result));
    NS_ENSURE_SUCCESS(result, result);

    target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
  }

  const nsAString& empty = EmptyString();
  result = mXMLHttpRequest->OpenRequest(NS_LITERAL_CSTRING("GET"),
                                        aURI, PR_TRUE, empty, empty);
  NS_ENSURE_SUCCESS(result, result);
   
  mXMLHttpRequest->OverrideMimeType(NS_LITERAL_CSTRING("text/xml"));

  return mXMLHttpRequest->Send(nsnull);

}
                        
nsresult
nsPolicyReference::ProcessPolicyReferenceFile(nsIDOMDocument* aDocument,
                                              char** aPolicyLocation)
{

  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aPolicyLocation);
    
  nsCOMPtr<nsIDOMElement> domElement;
  aDocument->GetDocumentElement(getter_AddRefs(domElement));

  nsCOMPtr<nsIDOMNode> root(do_QueryInterface(domElement));
  NS_ENSURE_TRUE(root, NS_ERROR_UNEXPECTED);

  nsAutoString name;
  root->GetNodeName(name);

  nsresult result = NS_OK;
  
  // The root element MUST be META
  mError = name.EqualsLiteral("META") 
    ? POLICY_LOAD_SUCCESS : POLICY_SYNTAX_ERROR;
 
  if (mError != POLICY_LOAD_SUCCESS)
    return result;
  
  nsCOMPtr<nsIDOMNodeList> policyReferencesElement;
  aDocument->GetElementsByTagName(NS_LITERAL_STRING("POLICY-REFERENCES"), 
                                  getter_AddRefs(policyReferencesElement));
  NS_ENSURE_TRUE(policyReferencesElement, NS_ERROR_UNEXPECTED);

  PRUint32 count;
  policyReferencesElement->GetLength(&count);

  // There MUST be exactly |one| POLICY-REFERENCES element
  mError = (count == 1) ? POLICY_LOAD_SUCCESS : POLICY_SYNTAX_ERROR;

  if (mError != POLICY_LOAD_SUCCESS)
    return result;

  nsCOMPtr<nsIDOMNodeList> expiryElement;
  aDocument->GetElementsByTagName(NS_LITERAL_STRING("EXPIRY"), 
                                  getter_AddRefs(expiryElement));

  result = ProcessExpiryElement(expiryElement);
  NS_ENSURE_SUCCESS(result, result);

  if (mError != POLICY_LOAD_SUCCESS)
    return result;

  nsCOMPtr<nsIDOMNodeList> policyRefElement;
  aDocument->GetElementsByTagName(NS_LITERAL_STRING("POLICY-REF"), 
                                  getter_AddRefs(policyRefElement));

  nsAutoString policyLocation;
  result = ProcessPolicyRefElement(aDocument, policyRefElement, policyLocation);
  NS_ENSURE_SUCCESS(result, result);
      
  if (mError != POLICY_LOAD_SUCCESS || policyLocation.IsEmpty())
      return result;

  nsAutoString absURI;
  if (mFlags & IS_LINKED_URI) {
    result = NS_MakeAbsoluteURI(absURI, policyLocation,  mLinkedURI);
    NS_ENSURE_SUCCESS(result, result);
  }
  else {
    // Make sure that the fragment identifier belongs to the current host.
    if (policyLocation.First() == '#') {
      policyLocation = PromiseFlatString(NS_LITERAL_STRING("p3p.xml") + policyLocation);
    }
    if (mFlags & IS_MAIN_URI) {
      nsCOMPtr<nsIURI> tmpURI= mMainURI;
      tmpURI->SetPath(NS_LITERAL_CSTRING(kW3C));
      result = NS_MakeAbsoluteURI(absURI, policyLocation,  tmpURI);
      NS_ENSURE_SUCCESS(result, result);
    }
    else {
      // it is ok to do this because we won't be needing current uri beyond this.
      mCurrentURI->SetPath(NS_LITERAL_CSTRING(kW3C));
      result = NS_MakeAbsoluteURI(absURI, policyLocation,  mCurrentURI);
      NS_ENSURE_SUCCESS(result, result);
    }
  }
  *aPolicyLocation = ToNewCString(absURI);
  NS_ENSURE_TRUE(*aPolicyLocation, NS_ERROR_OUT_OF_MEMORY);
 
  return result;
}

nsresult
nsPolicyReference::ProcessPolicyRefElement(nsIDOMDocument* aDocument, 
                                           nsIDOMNodeList* aNodeList, 
                                           nsAString& aPolicyLocation)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aNodeList);

  PRUint32 count;
  aNodeList->GetLength(&count);

  nsAutoString element;
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMNode> node;
    aNodeList->Item(i, getter_AddRefs(node));
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);
    
    nsresult result = ProcessPolicyRefChildren(node);
    NS_ENSURE_SUCCESS(result, result);

    if (mError == POLICY_LOAD_SUCCESS) {
      return nsP3PUtils::GetAttributeValue(node, "about", aPolicyLocation);
    }
  }

  return NS_OK;
}

nsresult 
nsPolicyReference::ProcessExpiryElement(nsIDOMNodeList* aNodeList)
{
  NS_ENSURE_ARG_POINTER(aNodeList);
  
  PRUint32 count;
  aNodeList->GetLength(&count);
  if (count > 0) {
    nsCOMPtr<nsIDOMNode> node;
    aNodeList->Item(0, getter_AddRefs(node)); // There ought to be only one EXPIRY element
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);
    
    nsAutoString date;
    nsP3PUtils::GetAttributeValue(node, "date", date); // absdate
    if (!date.IsEmpty()) {
      char* cdate = ToNewCString(date);
      NS_ENSURE_TRUE(*cdate, NS_ERROR_OUT_OF_MEMORY);
       
      PRTime prdate;
      if (PR_ParseTimeString(cdate, PR_TRUE, &prdate) == PR_SUCCESS) {
        if (prdate < PR_Now()) {
          mError = POLICY_LIFE_EXPIRED;
        }
      }
      nsMemory::Free(cdate);
    }
  }

  return NS_OK;
}

nsresult
nsPolicyReference::ProcessPolicyRefChildren(nsIDOMNode* aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsresult result = NS_OK;
  // When INCLUDE elements are present in a POLICY-REF element, it means that
  // the policy specified in the about attribute of the POLICY-REF element applies to
  // all the URIs at the requested host corresponding to the local-URI(s) matched by
  // any of the INCLUDES, but not matched by an EXCLUDE. It is legal, but pointles,
  // to supply the EXCLUDE element without any INCLUDE lements; in that case, the
  // EXCLUDE element MUST be ignored.

  nsAutoVoidArray elements;
  nsXPIDLCString path;
  mCurrentURI->GetPath(path);

  nsP3PUtils::GetElementsByTagName(aNode, NS_LITERAL_STRING("INCLUDE"), elements);
  CLEAN_ARRAY_AND_RETURN_IF_FAILED(result, elements);

  if (elements.Count() == 0) {
    mError = POLICY_LOAD_FAILURE;
    return NS_OK; // INCLUDE elements not found so no need to check for EXCLUDE
  }

  PRBool pathIncluded = PR_FALSE;
  result = nsP3PUtils::DeterminePolicyScope(elements, path, &pathIncluded);
  CLEAN_ARRAY_AND_RETURN_IF_FAILED(result, elements);

  mError = pathIncluded ? POLICY_LOAD_SUCCESS : POLICY_LOAD_FAILURE;
  if (mError == POLICY_LOAD_SUCCESS) {
    result = 
      nsP3PUtils::GetElementsByTagName(aNode, NS_LITERAL_STRING("EXCLUDE"), elements);
    CLEAN_ARRAY_AND_RETURN_IF_FAILED(result, elements);

    PRBool pathExcluded = PR_FALSE;
    result = nsP3PUtils::DeterminePolicyScope(elements, path, &pathExcluded);
    mError = pathExcluded ? POLICY_LOAD_FAILURE : POLICY_LOAD_SUCCESS;
  }
  nsP3PUtils::CleanArray(elements);
  
  return result;
}

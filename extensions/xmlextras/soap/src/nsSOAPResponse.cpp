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

#include "nsSOAPResponse.h"
#include "nsSOAPUtils.h"
#include "nsSOAPFault.h"
#include "nsISOAPParameter.h"

nsSOAPResponse::nsSOAPResponse()
{
}

nsSOAPResponse::~nsSOAPResponse()
{
  /* destructor code */
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPResponse, nsISOAPMessage, nsISOAPResponse)
NS_IMPL_ADDREF_INHERITED(nsSOAPResponse, nsSOAPMessage)
NS_IMPL_RELEASE_INHERITED(nsSOAPResponse, nsSOAPMessage)

NS_INTERFACE_MAP_BEGIN(nsSOAPResponse)
NS_INTERFACE_MAP_ENTRY(nsISOAPResponse)
NS_IMPL_QUERY_CLASSINFO(nsSOAPResponse)
NS_INTERFACE_MAP_END_INHERITING(nsSOAPMessage)


/* readonly attribute nsISOAPFault fault; */
NS_IMETHODIMP nsSOAPResponse::GetFault(nsISOAPFault * *aFault)
{
  NS_ENSURE_ARG_POINTER(aFault);
  nsCOMPtr<nsIDOMElement> body;

  *aFault = nsnull;
  GetBody(getter_AddRefs(body));
  if (body) {
    nsSOAPUtils::GetSpecificChildElement(body, 
      nsSOAPUtils::kSOAPEnvURI, nsSOAPUtils::kFaultTagName, 
      getter_AddRefs(body));
    if (body) {
      *aFault = new nsSOAPFault(body);
      if (!*aFault)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(*aFault);
    }
  }
  else {
    *aFault = nsnull;
  }
  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPResponse::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPResponse))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPResponse::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPResponse))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPResponse::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPResponse))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPResponse::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPResponse))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

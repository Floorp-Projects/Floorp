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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPParameter.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"

nsSOAPParameter::nsSOAPParameter()
{
  NS_INIT_ISUPPORTS();
}

nsSOAPParameter::~nsSOAPParameter()
{
}

NS_IMPL_ISUPPORTS3_CI(nsSOAPParameter, 
                      nsISOAPParameter, 
                      nsISecurityCheckedComponent,
                      nsIJSNativeInitializer)

/* attribute AString namespaceURI; */
NS_IMETHODIMP nsSOAPParameter::GetNamespaceURI(nsAWritableString & aNamespaceURI)
{
  NS_ENSURE_ARG_POINTER(&aNamespaceURI);
  if (mElement) {
    return mElement->GetNamespaceURI(aNamespaceURI);
  }
  else {
    aNamespaceURI.Assign(mNamespaceURI);
  }
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetNamespaceURI(const nsAReadableString & aNamespaceURI)
{
  if (mElement) {
    return NS_ERROR_FAILURE;
  }
  mNamespaceURI.Assign(aNamespaceURI);
  return NS_OK;
}

/* attribute AString name; */
NS_IMETHODIMP nsSOAPParameter::GetName(nsAWritableString & aName)
{
  NS_ENSURE_ARG_POINTER(&aName);
  if (mElement) {
    return mElement->GetLocalName(aName);
  }
  else {
    aName.Assign(mName);
  }
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetName(const nsAReadableString & aName)
{
  if (mElement) {
    return NS_ERROR_FAILURE;
  }
  mName.Assign(aName);
  return NS_OK;
}

/* attribute nsISOAPEncoding encoding; */
NS_IMETHODIMP nsSOAPParameter::GetEncoding(nsISOAPEncoding* * aEncoding)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  *aEncoding = mEncoding;
  NS_IF_ADDREF(*aEncoding);
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetEncoding(nsISOAPEncoding* aEncoding)
{
  mEncoding = aEncoding;
  if (mElement) {
    mComputeValue = PR_TRUE;
    mValue = nsnull;
    mStatus = NS_OK;
  }
  return NS_OK;
}

/* attribute nsISchemaType schemaType; */
NS_IMETHODIMP nsSOAPParameter::GetSchemaType(nsISchemaType* * aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);
  *aSchemaType = mSchemaType;
  NS_IF_ADDREF(*aSchemaType);
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetSchemaType(nsISchemaType* aSchemaType)
{
  mSchemaType = aSchemaType;
  if (mElement) {
    mComputeValue = PR_TRUE;
    mValue = nsnull;
    mStatus = NS_OK;
  }
  return NS_OK;
}

/* attribute nsISOAPAttachments attachments; */
NS_IMETHODIMP nsSOAPParameter::GetAttachments(nsISOAPAttachments* * aAttachments)
{
  NS_ENSURE_ARG_POINTER(aAttachments);
  *aAttachments = mAttachments;
  NS_IF_ADDREF(*aAttachments);
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetAttachments(nsISOAPAttachments* aAttachments)
{
  mAttachments = aAttachments;
  if (mElement) {
    mComputeValue = PR_TRUE;
    mValue = nsnull;
    mStatus = NS_OK;
  }
  return NS_OK;
}

/* attribute nsIDOMElement element; */
NS_IMETHODIMP nsSOAPParameter::GetElement(nsIDOMElement* * aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = mElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetElement(nsIDOMElement* aElement)
{
  mElement = aElement;
  mNamespaceURI.SetLength(0);
  mName.SetLength(0);
  mComputeValue = PR_TRUE;
  mValue = nsnull;
  mStatus = NS_OK;
  return NS_OK;
}

/* attribute nsIVariant value; */
NS_IMETHODIMP nsSOAPParameter::GetValue(nsIVariant* * aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  if (mElement    //  Check for auto-computation
    && mComputeValue
    && mEncoding)
  {
    mComputeValue = PR_FALSE;
    mStatus = mEncoding->Decode(mElement, mSchemaType, mAttachments, getter_AddRefs(mValue));
  }
  *aValue = mValue;
  NS_IF_ADDREF(*aValue);
  return mElement ? mStatus : NS_OK;
}
NS_IMETHODIMP nsSOAPParameter::SetValue(nsIVariant* aValue)
{
  mValue = aValue;
  mComputeValue = PR_FALSE;
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsSOAPParameter::Initialize(JSContext *cx, JSObject *obj, 
                            PRUint32 argc, jsval *argv)
{

//  Get the arguments.

  nsCOMPtr<nsIVariant>  value;
  nsAutoString          name;
  nsAutoString          namespaceURI;
  nsCOMPtr<nsISupports> schemaType;
  nsCOMPtr<nsISupports> encoding;

  if (!JS_ConvertArguments(cx, argc, argv, "/%iv %is %is %ip %ip", 
    getter_AddRefs(value), 
    NS_STATIC_CAST(nsAString*, &name), 
    NS_STATIC_CAST(nsAString*, &namespaceURI), 
    getter_AddRefs(schemaType),
    getter_AddRefs(encoding))) return NS_ERROR_ILLEGAL_VALUE;

  nsresult rc = SetValue(value);
  if (NS_FAILED(rc)) return rc;
  rc = SetName(name);
  if (NS_FAILED(rc)) return rc;
  rc = SetNamespaceURI(namespaceURI);
  if (NS_FAILED(rc)) return rc;
  if (schemaType) {
    nsCOMPtr<nsISchemaType> v = do_QueryInterface(schemaType, &rc);
    if (NS_FAILED(rc)) return rc;
    rc = SetSchemaType(v);
    if (NS_FAILED(rc)) return rc;
  }
  if (encoding) {
    nsCOMPtr<nsISOAPEncoding> v = do_QueryInterface(encoding, &rc);
    if (NS_FAILED(rc)) return rc;
    rc = SetEncoding(v);
    if (NS_FAILED(rc)) return rc;
  }

  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPParameter::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPParameter::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPParameter::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPParameter::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPParameter))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

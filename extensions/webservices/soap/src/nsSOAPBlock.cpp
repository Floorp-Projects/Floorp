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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPBlock.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"
#include "nsISOAPMessage.h"
#include "nsSOAPException.h"

nsSOAPBlock::nsSOAPBlock()
{
}

nsSOAPBlock::~nsSOAPBlock()
{
}

NS_IMPL_ISUPPORTS2(nsSOAPBlock, nsISOAPBlock, nsIJSNativeInitializer)
NS_IMETHODIMP nsSOAPBlock::Init(nsISOAPAttachments * aAttachments,
                                PRUint16 aVersion)
{
  if (aVersion == nsISOAPMessage::VERSION_1_1
      || aVersion == nsISOAPMessage::VERSION_1_2) {
    mAttachments = aAttachments;
    mVersion = aVersion;
    return NS_OK;
  }
  return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_BAD_VERSION", "Bad version used to initialize block.");
}

/* attribute AString namespaceURI; */
NS_IMETHODIMP nsSOAPBlock::GetNamespaceURI(nsAString & aNamespaceURI)
{
  if (mElement) {
    if (mEncoding) {
      nsAutoString temp;
      nsresult rc = mElement->GetNamespaceURI(temp);
      if (NS_FAILED(rc))
        return rc;
      return mEncoding->GetInternalSchemaURI(temp, aNamespaceURI);
    }
    return mElement->GetNamespaceURI(aNamespaceURI);
  } else {
    aNamespaceURI.Assign(mNamespaceURI);
  }
  return NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetNamespaceURI(const nsAString & aNamespaceURI)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc))
    return rc;
  mNamespaceURI.Assign(aNamespaceURI);
  return NS_OK;
}

/* attribute AString name; */
NS_IMETHODIMP nsSOAPBlock::GetName(nsAString & aName)
{
  if (mElement) {
    return mElement->GetLocalName(aName);
  } else {
    aName.Assign(mName);
  }
  return NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetName(const nsAString & aName)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc))
    return rc;
  mName.Assign(aName);
  return NS_OK;
}

/* attribute nsISOAPEncoding encoding; */
NS_IMETHODIMP nsSOAPBlock::GetEncoding(nsISOAPEncoding * *aEncoding)
{
  NS_ENSURE_ARG_POINTER(aEncoding);
  *aEncoding = mEncoding;
  NS_IF_ADDREF(*aEncoding);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetEncoding(nsISOAPEncoding * aEncoding)
{
  mEncoding = aEncoding;
  mComputeValue = PR_TRUE;
  return NS_OK;
}

/* attribute nsISchemaType schemaType; */
NS_IMETHODIMP nsSOAPBlock::GetSchemaType(nsISchemaType * *aSchemaType)
{
  NS_ENSURE_ARG_POINTER(aSchemaType);
  *aSchemaType = mSchemaType;
  NS_IF_ADDREF(*aSchemaType);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetSchemaType(nsISchemaType * aSchemaType)
{
  mSchemaType = aSchemaType;
  mComputeValue = PR_TRUE;
  return NS_OK;
}

/* attribute nsIDOMElement element; */
NS_IMETHODIMP nsSOAPBlock::GetElement(nsIDOMElement * *aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = mElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetElement(nsIDOMElement * aElement)
{
  mElement = aElement;
  mComputeValue = PR_TRUE;
  return NS_OK;
}

/* attribute nsIVariant value; */
NS_IMETHODIMP nsSOAPBlock::GetValue(nsIVariant * *aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  if (mElement) {                //  Check for auto-computation
    if (mComputeValue) {
      mComputeValue = PR_FALSE;
      if (mEncoding) {
        mStatus =
            mEncoding->Decode(mElement, mSchemaType, mAttachments,
                              getter_AddRefs(mValue));
      } else {
        mStatus = SOAP_EXCEPTION(NS_ERROR_NOT_INITIALIZED,"SOAP_NO_ENCODING","No encoding found to decode block.");
      }
    }
  }
  *aValue = mValue;
  NS_IF_ADDREF(*aValue);
  return mElement ? mStatus : NS_OK;
}

NS_IMETHODIMP nsSOAPBlock::SetValue(nsIVariant * aValue)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc))
    return rc;
  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP
    nsSOAPBlock::Initialize(JSContext * cx, JSObject * obj,
                            PRUint32 argc, jsval * argv)
{

//  Get the arguments.


  nsAutoString name;
  nsAutoString namespaceURI;
  nsIVariant* s1 = nsnull;
  nsISupports* s2 = nsnull;
  nsISupports* s3 = nsnull;

  if (!JS_ConvertArguments(cx, argc, argv, "/%iv %is %is %ip %ip",
                           &s1,
                           NS_STATIC_CAST(nsAString *, &name),
                           NS_STATIC_CAST(nsAString *, &namespaceURI),
                           &s2,
                           &s3))
    return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_BLOCK_INIT", "Could not interpret block initialization arguments.");
  
  nsCOMPtr<nsIVariant> value = dont_AddRef(s1);
  nsCOMPtr<nsISupports> schemaType = dont_AddRef(s2);
  nsCOMPtr<nsISupports> encoding = dont_AddRef(s3);

  nsresult rc = SetValue(value);
  if (NS_FAILED(rc))
    return rc;
  rc = SetName(name);
  if (NS_FAILED(rc))
    return rc;
  rc = SetNamespaceURI(namespaceURI);
  if (NS_FAILED(rc))
    return rc;
  if (schemaType) {
    nsCOMPtr<nsISchemaType> v = do_QueryInterface(schemaType, &rc);
    if (NS_FAILED(rc))
      return rc;
    rc = SetSchemaType(v);
    if (NS_FAILED(rc))
      return rc;
  }
  if (encoding) {
    nsCOMPtr<nsISOAPEncoding> v = do_QueryInterface(encoding, &rc);
    if (NS_FAILED(rc))
      return rc;
    rc = SetEncoding(v);
    if (NS_FAILED(rc))
      return rc;
  }

  return NS_OK;
}

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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPHeaderBlock.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"

nsSOAPHeaderBlock::nsSOAPHeaderBlock()
{
  NS_INIT_ISUPPORTS();
}

nsSOAPHeaderBlock::nsSOAPHeaderBlock(nsISOAPAttachments* aAttachments): nsSOAPBlock(aAttachments)
{
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPHeaderBlock, nsISOAPBlock, nsISOAPHeaderBlock)
NS_IMPL_ADDREF_INHERITED(nsSOAPHeaderBlock, nsSOAPBlock)
NS_IMPL_RELEASE_INHERITED(nsSOAPHeaderBlock, nsSOAPBlock)

NS_INTERFACE_MAP_BEGIN(nsSOAPHeaderBlock)
NS_INTERFACE_MAP_ENTRY(nsISOAPHeaderBlock)
NS_IMPL_QUERY_CLASSINFO(nsSOAPHeaderBlock)
NS_INTERFACE_MAP_END_INHERITING(nsSOAPBlock)

nsSOAPHeaderBlock::~nsSOAPHeaderBlock()
{
}

/* attribute AString actorURI; */
NS_IMETHODIMP nsSOAPHeaderBlock::GetActorURI(nsAWritableString & aActorURI)
{
  NS_ENSURE_ARG_POINTER(&aActorURI);
  if (mElement) {
    return mElement->GetAttributeNS(nsSOAPUtils::kSOAPEnvURI,nsSOAPUtils::kActorAttribute,aActorURI);
  }
  else {
    aActorURI.Assign(mActorURI);
  }
  return NS_OK;
}
NS_IMETHODIMP nsSOAPHeaderBlock::SetActorURI(const nsAReadableString & aActorURI)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc)) return rc;
  mActorURI.Assign(aActorURI);
  return NS_OK;
}

/* attribute AString mustUnderstand; */
NS_IMETHODIMP nsSOAPHeaderBlock::GetMustUnderstand(PRBool * aMustUnderstand)
{
  NS_ENSURE_ARG_POINTER(&aMustUnderstand);
  if (mElement) {
    nsAutoString m;
    nsresult rc = mElement->GetAttributeNS(nsSOAPUtils::kSOAPEnvURI,nsSOAPUtils::kMustUnderstandAttribute,m);
    if (NS_FAILED(rc)) return rc;
    if (m.Length() == 0) *aMustUnderstand = PR_FALSE;
    else if (m.Equals(nsSOAPUtils::kTrueA) || m.Equals(nsSOAPUtils::kTrueA)) *aMustUnderstand = PR_TRUE;
    else if (m.Equals(nsSOAPUtils::kFalseA) || m.Equals(nsSOAPUtils::kFalseA)) *aMustUnderstand = PR_FALSE;
    else return NS_ERROR_ILLEGAL_VALUE;
    return NS_OK;
  }
  else {
    *aMustUnderstand = mMustUnderstand;
  }
  return NS_OK;
}
NS_IMETHODIMP nsSOAPHeaderBlock::SetMustUnderstand(PRBool aMustUnderstand)
{
  nsresult rc = SetElement(nsnull);
  if (NS_FAILED(rc)) return rc;
  mMustUnderstand = aMustUnderstand;
  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPHeaderBlock::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPHeaderBlock))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPHeaderBlock::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPHeaderBlock))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPHeaderBlock::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPHeaderBlock))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPHeaderBlock::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPHeaderBlock))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}
